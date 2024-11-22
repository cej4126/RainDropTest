#include <unordered_map>
#include "Lights.h"
#include "Helpers.h"
#include "Shaders.h"
#include "Resources.h"
#include "SharedTypes.h"
#include "Entity.h"
#include "AppItems.h"
#include "Math.h"
#include "Transform.h"
#include "GraphicPass.h"

namespace lights
{
    namespace {

        template<UINT n>
        struct u32_set_bits {
            static_assert(n > 0 && n < 32);
            constexpr static const UINT bits{ u32_set_bits<n - 1>::bits | (1 << (n - 1)) };
        };

        template<>
        struct u32_set_bits<0> {
            constexpr static const UINT bits{ 0 };
        };

        static_assert(u32_set_bits<Frame_Count>::bits < (1 << 8), "That's quite a large frame buffer count!");

        constexpr UINT8 dirty_bits_mask{ (UINT8)u32_set_bits<Frame_Count>::bits };

        // D#D12Light.cpp
        class LightSet
        {
        public:
            constexpr Light add(const light_init_info& info)
            {
                // directional light
                if (info.type == light_type::directional)
                {
                    // directional
                    UINT index{ Invalid_Index };
                    // Find an available slot in the array if any.
                    for (UINT i{ 0 }; i < m_non_cullable_owners_ids.size(); ++i)
                    {
                        if (m_non_cullable_owners_ids[i] != Invalid_Index)
                        {
                            index = i;
                            break;
                        }
                    }

                    if (index == Invalid_Index)
                    {
                        // not found, add to end of list
                        index = (UINT)m_non_cullable_owners_ids.size();
                        m_non_cullable_owners_ids.emplace_back();
                        m_non_cullable_lights.emplace_back();
                    }

                    hlsl::DirectionalLightParameters& params{ m_non_cullable_lights[index] };
                    params.Color = info.color;
                    params.Intensity = info.intensity;

                    light_owner owner{ info.entity_id, index, info.type, info.is_enabled };
                    const UINT id{ m_owners.add(owner) };
                    m_non_cullable_owners_ids[index] = id;

                    return Light{ id, info.set_key };
                }
                else
                {
                    // cullable light
                    UINT index{ Invalid_Index };
                    for (UINT i{ m_enabled_light_count }; i < m_cullable_owner_ids.size(); i++)
                    {
                        if (m_cullable_owner_ids[i] != Invalid_Index)
                        {
                            index = i;
                            break;
                        }
                    }

                    if (index == Invalid_Index)
                    {
                        index = (UINT)m_cullable_owner_ids.size();
                        m_cullable_lights.emplace_back();
                        m_culling_info.emplace_back();
                        m_bounding_spheres.emplace_back();
                        m_cullable_entity_ids.emplace_back();
                        m_cullable_owner_ids.emplace_back();
                        m_dirty_bits.emplace_back();

#ifdef _DEBUG
                        assert(m_cullable_owner_ids.size() == m_cullable_lights.size());
                        assert(m_cullable_owner_ids.size() == m_culling_info.size());
                        assert(m_cullable_owner_ids.size() == m_bounding_spheres.size());
                        assert(m_cullable_owner_ids.size() == m_cullable_entity_ids.size());
                        assert(m_cullable_owner_ids.size() == m_dirty_bits.size());
#endif
                    }

                    //add_cullable_light_parameters(info, index);
                    hlsl::LightParameters& params{ m_cullable_lights[index] };

                    params.Color = info.color;
                    params.Intensity = info.intensity;

                    params.Attenuation = info.attenuation;
                    params.Range = info.range;

                    params.CosUmbra = XMScalarCos(info.spot_params.umbra * 0.5f);
                    params.CosPenumbra = XMScalarCos(info.spot_params.penumbra * 0.5f);

                    //add_light_culling_info(info, index);
                    hlsl::LightCullingLightInfo& culling_info{ m_culling_info[index] };
                    m_bounding_spheres[index].Radius = params.Range;
                    culling_info.Range = params.Range;
                    culling_info.CosPenumbra = -1.f;

                    if (info.type == light_type::spot)
                    {
                        culling_info.CosPenumbra = params.CosPenumbra;
                    }

                    const UINT id{ m_owners.add(light_owner{info.entity_id, index, info.type, info.is_enabled }) };
                    m_cullable_entity_ids[index] = m_owners[id].entity_id;
                    m_cullable_owner_ids[index] = id;
                    make_dirty(index);
                    enable(id, info.is_enabled);
                    update_transform(index);

                    return Light{ id, info.set_key };
                }
            }

            void remove(UINT id)
            {
                enable(id, false);
                const light_owner& owner{ m_owners[id] };

                if (owner.type == light_type::directional)
                {
                    m_non_cullable_owners_ids[owner.light_index] = Invalid_Index;
                }
                else
                {
                    assert(m_owners[m_cullable_owner_ids[owner.light_index]].light_index == owner.light_index);
                    m_cullable_owner_ids[owner.light_index] = Invalid_Index;
                }

                m_owners.remove(id);
            }

            void update_transforms()
            {
                // Update direction of directional light
                for (const auto& id : m_non_cullable_owners_ids)
                {
                    if (id == Invalid_Index)
                    {
                        continue;
                    }

                    const light_owner& owner{ m_owners[id] };
                    if (owner.is_enabled)
                    {
                        game_entity::entity entity{ owner.entity_id };
                        hlsl::DirectionalLightParameters& params{ m_non_cullable_lights[owner.light_index] };
                        params.Direction = entity.orientation();
                    }
                }

                // Update position and direction of cullable lights
                const UINT count{ m_enabled_light_count };
                if (count)
                {
                    assert(m_cullable_entity_ids.size() >= count);
                    m_transform_flags_cache.resize(count);
                    transform::get_updated_components_flags(m_cullable_entity_ids.data(), count, m_transform_flags_cache.data());

                    for (UINT i{ 0 }; i < count; ++i)
                    {
                        if (m_transform_flags_cache[i])
                        {
                            update_transform(i);
                        }
                    }
                }
            }

            constexpr UINT non_cullable_light_count() const
            {
                UINT count{ 0 };
                for (const auto& id : m_non_cullable_owners_ids)
                {
                    if (id != Invalid_Index && m_owners[id].is_enabled)
                    {
                        ++count;
                    }
                }

                return count;
            }

            constexpr void non_cullable_lights(hlsl::DirectionalLightParameters* const lights, UINT buffer_size) const
            {
                assert(buffer_size >= math::align_size_up<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(non_cullable_light_count() * sizeof(hlsl::DirectionalLightParameters)));
                const UINT count{ (UINT)m_non_cullable_owners_ids.size() };
                UINT index{ 0 };
                for (UINT i{ 0 }; i < count; ++i)
                {
                    if (m_non_cullable_owners_ids[i] != Invalid_Index) continue;

                    const light_owner& owner{ m_owners[m_non_cullable_owners_ids[i]] };
                    if (owner.is_enabled)
                    {
                        assert(m_owners[m_non_cullable_owners_ids[i]].light_index == i);
                        lights[index] = m_non_cullable_lights[i];
                        ++index;
                    }
                }
            }

            constexpr UINT cullable_light_count() const
            {
                return m_enabled_light_count;
            }


            void enable(UINT id, bool is_enabled)
            {
                // Set the owner enable state

                // Directional light are not packed
                if (m_owners[id].type == light_type::directional)
                {
                    return;
                }

                // Packed the cullable light, swap the disable lights after the enable lights
                const UINT light_index{ m_owners[id].light_index };

                if (is_enabled)
                {
                    // enable light
                    if (light_index > m_enabled_light_count)
                    {
                        // light was disabled
                        assert(m_enabled_light_count < m_cullable_lights.size());
                        swap_cullable_lights(light_index, m_enabled_light_count);
                        ++m_enabled_light_count;
                    }
                    else if (light_index == m_enabled_light_count)
                    {
                        // light is the first item in the disable list. No swapping needed
                        ++m_enabled_light_count;
                    }
                    // else is already enable
                }
                else
                {
                    // disable light
                    const UINT last{ m_enabled_light_count - 1 };
                    if (light_index < last)
                    {
                        // light was enable
                        swap_cullable_lights(light_index, last);
                        --m_enabled_light_count;
                    }
                    else if (light_index == last)
                    {
                        // light is the first item in the disable list. No swapping needed
                        --m_enabled_light_count;
                    }
                    // else is already disenable
                }

            }

            constexpr bool has_lights() const
            {
                return m_owners.size();
            }

        private:

            void calculate_cone_bounding_sphere(const hlsl::LightParameters& params, hlsl::Sphere& sphere)
            {
                XMVECTOR tip{ XMLoadFloat3(&params.Position) };
                XMVECTOR direction{ XMLoadFloat3(&params.Direction) };
                const float cone_cos{ params.CosPenumbra };
                assert(cone_cos > 0.f);

                if (cone_cos >= 0.707107f)
                {
                    sphere.Radius = params.Range / (2.f * cone_cos);
                    XMStoreFloat3(&sphere.Center, tip + sphere.Radius * direction);
                }
                else
                {
                    XMStoreFloat3(&sphere.Center, tip + cone_cos * params.Range * direction);
                    const float cone_sin{ sqrt(1.f - cone_cos * cone_cos) };
                    sphere.Radius = cone_sin * params.Range;
                }
            }

            void update_transform(UINT index)
            {
                game_entity::entity entity{ m_cullable_entity_ids[index] };
                hlsl::LightParameters& light_params{ m_cullable_lights[index] };
                light_params.Position = entity.position();

                hlsl::LightCullingLightInfo& culling_info{ m_culling_info[index] };
                culling_info.Position = entity.position();
                m_bounding_spheres[index].Center = entity.position();

                if (m_owners[m_cullable_owner_ids[index]].type == light_type::spot)
                {
                    culling_info.Direction = entity.orientation();
                    light_params.Direction = entity.orientation();
                    calculate_cone_bounding_sphere(light_params, m_bounding_spheres[index]);
                }

                make_dirty(index);
            }

            void swap_cullable_lights(UINT index1, UINT index2)
            {
                assert(index1 != index2);
                assert(index1 < m_cullable_owner_ids.size());
                assert(index2 < m_cullable_owner_ids.size());
                // verify one light is valid
                assert(m_cullable_owner_ids[index1] != Invalid_Index || m_cullable_owner_ids[index2] != Invalid_Index);


                if (m_cullable_owner_ids[index2] == Invalid_Index)
                {
                    // second light is not valid. This will reduce code checks
                    std::swap(index1, index2);
                }

                if (m_cullable_owner_ids[index1] == Invalid_Index)
                {
                    // only index2 is valid
                    light_owner& owner2{ m_owners[m_cullable_owner_ids[index2]] };
                    assert(owner2.light_index == index2);
                    owner2.light_index = index1;

                    m_cullable_lights[index1] = m_cullable_lights[index2];
                    m_culling_info[index1] = m_culling_info[index2];
                    m_bounding_spheres[index1] = m_bounding_spheres[index2];
                    m_cullable_entity_ids[index1] = m_cullable_entity_ids[index2];
                    std::swap(m_cullable_owner_ids[index1], m_cullable_owner_ids[index2]);
                    make_dirty(index1);
                    assert(m_owners[m_cullable_owner_ids[index1]].entity_id == m_cullable_entity_ids[index1]);
                    assert(m_cullable_owner_ids[index2] == Invalid_Index);
                }
                else
                {
                    // both index1 and index2 are valid
                    light_owner& owner1{ m_owners[m_cullable_owner_ids[index1]] };
                    light_owner& owner2{ m_owners[m_cullable_owner_ids[index2]] };
                    assert(owner1.light_index == index1);
                    assert(owner2.light_index == index2);
                    owner1.light_index = owner2.light_index;
                    owner2.light_index = owner1.light_index;

                    std::swap(m_cullable_lights[index1], m_cullable_lights[index2]);
                    std::swap(m_culling_info[index1], m_culling_info[index2]);
                    std::swap(m_bounding_spheres[index1], m_bounding_spheres[index2]);
                    std::swap(m_cullable_entity_ids[index1], m_cullable_entity_ids[index2]);
                    std::swap(m_owners[index1], m_owners[index2]);

                    assert(m_owners[m_cullable_owner_ids[index1]].entity_id == m_cullable_entity_ids[index1]);
                    assert(m_owners[m_cullable_owner_ids[index2]].entity_id == m_cullable_entity_ids[index2]);

                    // set dirty bits
                    make_dirty(index1);
                    make_dirty(index2);
                }
            }

            constexpr void make_dirty(UINT index)
            {
                assert(index < m_dirty_bits.size());
                m_something_is_dirty = dirty_bits_mask;
                m_dirty_bits[index] = dirty_bits_mask;
            }

            utl::free_list<light_owner> m_owners;
            utl::vector<hlsl::DirectionalLightParameters> m_non_cullable_lights;
            utl::vector<UINT> m_non_cullable_owners_ids;

            // Packed
            utl::vector<hlsl::LightParameters> m_cullable_lights;
            utl::vector<hlsl::LightCullingLightInfo> m_culling_info;
            utl::vector<hlsl::Sphere> m_bounding_spheres;
            utl::vector<UINT> m_cullable_entity_ids;
            utl::vector<UINT> m_cullable_owner_ids;
            utl::vector<UINT8> m_dirty_bits;

            utl::vector<UINT8> m_transform_flags_cache;
            UINT m_enabled_light_count{ 0 };
            UINT8 m_something_is_dirty{ 0 };

            friend class LightBuffer;
        };

        // D#D12Light.cpp
        class LightBuffer
        {
        public:
            LightBuffer() = default;
            constexpr void update_light_buffer(LightSet& light_set, UINT64 light_set_key, UINT frame_index)
            {
                // Process the non cullable lights

                const UINT non_cullable_light_count{ light_set.non_cullable_light_count() };

                if (non_cullable_light_count)
                {
                    const UINT needed_size{ non_cullable_light_count * sizeof(hlsl::DirectionalLightParameters) };
                    const UINT current_size{ m_buffers[light_buffer::non_cullable_light].buffer.size() };

                    if (current_size < needed_size)
                    {
                        resize_buffer(light_buffer::non_cullable_light, needed_size, frame_index);
                    }

                    light_set.non_cullable_lights((hlsl::DirectionalLightParameters* const)m_buffers[light_buffer::non_cullable_light].cpu_address,
                        m_buffers[light_buffer::non_cullable_light].buffer.size());
                }

                // Process the cullable lights
                const UINT cullable_light_count{ light_set.cullable_light_count() };
                if (cullable_light_count)
                {
                    const UINT needed_light_buffer_size{ cullable_light_count * sizeof(hlsl::LightParameters) };
                    const UINT needed_culling_info_buffer_size{ cullable_light_count * sizeof(hlsl::LightCullingLightInfo) };
                    const UINT needed_spheres_buffer_size{ cullable_light_count * sizeof(hlsl::Sphere) };
                    const UINT current_light_buffer_size{ m_buffers[light_buffer::cullable_light].buffer.size() };

                    bool buffers_resized{ false };
                    if (current_light_buffer_size < needed_light_buffer_size)
                    {
                        // NOTE: we create buffers about 150% larger than needed to avoid recreating them
                        //       every time a few lights are added.
                        resize_buffer(light_buffer::cullable_light, (needed_light_buffer_size * 3) >> 1, frame_index);
                        resize_buffer(light_buffer::culling_info, (needed_culling_info_buffer_size * 3) >> 1, frame_index);
                        resize_buffer(light_buffer::bounding_spheres, (needed_spheres_buffer_size * 3) >> 1, frame_index);
                        buffers_resized = true;
                    }

                    const UINT index_mask{ 1UL << frame_index };

                    if (buffers_resized || m_current_light_set_key != light_set_key)
                    {
                        memcpy(m_buffers[light_buffer::cullable_light].cpu_address, light_set.m_cullable_lights.data(), needed_light_buffer_size);
                        memcpy(m_buffers[light_buffer::culling_info].cpu_address, light_set.m_culling_info.data(), needed_culling_info_buffer_size);
                        memcpy(m_buffers[light_buffer::bounding_spheres].cpu_address, light_set.m_bounding_spheres.data(), needed_spheres_buffer_size);
                        m_current_light_set_key = light_set_key;

                        for (UINT i{ 0 }; i < cullable_light_count; ++i)
                        {
                            light_set.m_dirty_bits[i] &= ~index_mask;
                        }
                    }
                    else if (light_set.m_something_is_dirty)
                    {
                        for (UINT i{ 0 }; i < cullable_light_count; ++i)
                        {
                            if (light_set.m_dirty_bits[i] & index_mask)
                            {
                                assert(i * sizeof(hlsl::LightParameters) < needed_light_buffer_size);
                                assert(i * sizeof(hlsl::LightCullingLightInfo) < needed_culling_info_buffer_size);
                                UINT8* const light_dst{ m_buffers[light_buffer::cullable_light].cpu_address + (i * sizeof(hlsl::LightParameters)) };
                                UINT8* const culling_dst{ m_buffers[light_buffer::culling_info].cpu_address + (i * sizeof(hlsl::LightCullingLightInfo)) };
                                UINT8* const bounding_dst{ m_buffers[light_buffer::bounding_spheres].cpu_address + (i * sizeof(hlsl::Sphere)) };
                                memcpy(light_dst, &light_set.m_cullable_lights[i], sizeof(hlsl::LightParameters));
                                memcpy(culling_dst, &light_set.m_culling_info[i], sizeof(hlsl::LightCullingLightInfo));
                                memcpy(bounding_dst, &light_set.m_bounding_spheres[i], sizeof(hlsl::Sphere));
                                light_set.m_dirty_bits[i] &= ~index_mask;
                            }
                        }
                    }

                    light_set.m_something_is_dirty &= ~index_mask;
                    assert(m_current_light_set_key == light_set_key);
                }
            }

            constexpr void release()
            {
                for (UINT i{ 0 }; i < light_buffer::count; ++i)
                {
                    m_buffers[i].buffer.release();
                    m_buffers[i].cpu_address = nullptr;
                }
            }

            constexpr D3D12_GPU_VIRTUAL_ADDRESS non_cullable_lights() const { return m_buffers[light_buffer::non_cullable_light].buffer.gpu_address(); }
            constexpr D3D12_GPU_VIRTUAL_ADDRESS cullable_lights() const { return m_buffers[light_buffer::cullable_light].buffer.gpu_address(); }
            constexpr D3D12_GPU_VIRTUAL_ADDRESS culling_info() const { return m_buffers[light_buffer::culling_info].buffer.gpu_address(); }
            constexpr D3D12_GPU_VIRTUAL_ADDRESS bounding_spheres() const { return m_buffers[light_buffer::bounding_spheres].buffer.gpu_address(); }

        private:
            struct light_buffer
            {
                enum type : UINT {
                    non_cullable_light,
                    cullable_light,
                    culling_info,
                    bounding_spheres,

                    count
                };

                resource::Buffer buffer{};
                UINT8* cpu_address{ nullptr };
            };

            void resize_buffer(light_buffer::type type, UINT size, UINT frame_index)
            {
                assert(type < light_buffer::count && size);
                if (m_buffers[type].buffer.size() >= math::align_size_up<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(size))
                {
                    // buffer size is good
                    return;
                }

                resource::buffer_init_info info{};
                info.size = size;
                info.alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
                info.cpu_accessible = true;
                m_buffers[type].buffer = resource::Buffer{ info };
                NAME_D3D12_OBJECT_INDEXED(m_buffers[type].buffer.buffer(), frame_index,
                    type == light_buffer::non_cullable_light ? L"Non-cullable Light Buffer" :
                    type == light_buffer::cullable_light ? L"Cullable Light Buffer" :
                    type == light_buffer::culling_info ? L"Light Culling Info Buffer" : L"Bounding Spheres Buffer");

                D3D12_RANGE range{};
                ThrowIfFailed(m_buffers[type].buffer.buffer()->Map(0, &range, (void**)(&m_buffers[type].cpu_address)));
                assert(m_buffers[type].cpu_address);
            }

            light_buffer m_buffers[light_buffer::count]{};
            UINT64 m_current_light_set_key{ 0 };
        };

        struct light_culling_root_parameter {
            enum parameter : UINT {
                global_shader_data,
                constants,
                frustums_out_or_index_counter,
                frustums_in,
                culling_info,
                bounding_spheres,
                light_grid_opaque,
                light_index_list_opaque,

                count
            };
        };

        struct culling_parameters
        {
            resource::Buffer frustums;
            resource::Buffer light_grid_and_index_list;
            resource::uav_buffer light_index_counter;
            hlsl::LightCullingDispatchParameters grid_frustums_dispatch_params{};
            hlsl::LightCullingDispatchParameters light_culling_dispatch_params{};
            UINT frustum_count{ 0 };
            UINT view_width{ 0 };
            UINT view_height{ 0 };
            float camera_fov{ 0 };
            D3D12_GPU_VIRTUAL_ADDRESS light_index_list_opaque_buffer{ 0 };
            bool has_lights{ true };
        };

        struct light_culler
        {
            culling_parameters cullers[Frame_Count]{};
        };

        const UINT max_lights_per_tile{ 256 };

        std::unordered_map<UINT64, LightSet> m_light_set_keys;
        LightBuffer light_buffers[Frame_Count];

        ID3D12RootSignature* light_culling_root_signature{ nullptr };
        ID3D12PipelineState* light_culling_pso{ nullptr };
        ID3D12PipelineState* grid_frustum_pso{ nullptr };
        utl::free_list<light_culler> light_cullers;

        Light create_light(light_init_info info)
        {
            assert(m_light_set_keys.count(info.set_key));
            assert(info.entity_id != Invalid_Index);
            return m_light_set_keys[info.set_key].add(info);
        }


        void remove_light(UINT id, UINT64 light_set_key)
        {
            assert(m_light_set_keys.count(light_set_key));
            m_light_set_keys[light_set_key].remove(id);
        }

        struct light_set_states {
            enum state {
                left_set,
                right_set
            };
        };

        utl::vector<Light> lights;
        utl::vector<Light> disabled_lights;

        constexpr XMFLOAT3 rgb_to_color(UINT8 r, UINT8 g, UINT8 b)
        {
            return  { r / 255.f, g / 255.f , b / 255.f };
        }

        // D3D12LightCullling.cpp
        void create_root_signature_pso()
        {
            assert(!light_culling_root_signature);
            using param = light_culling_root_parameter;
            d3dx::d3d12_root_parameter parameters[param::count]{};
            parameters[param::global_shader_data].as_cbv(D3D12_SHADER_VISIBILITY_ALL, 0);
            parameters[param::constants].as_cbv(D3D12_SHADER_VISIBILITY_ALL, 1);

            parameters[param::frustums_out_or_index_counter].as_uav(D3D12_SHADER_VISIBILITY_ALL, 0);
            parameters[param::light_grid_opaque].as_uav(D3D12_SHADER_VISIBILITY_ALL, 1);
            parameters[param::light_index_list_opaque].as_uav(D3D12_SHADER_VISIBILITY_ALL, 3);

            parameters[param::frustums_in].as_srv(D3D12_SHADER_VISIBILITY_ALL, 0);
            parameters[param::culling_info].as_srv(D3D12_SHADER_VISIBILITY_ALL, 1);
            parameters[param::bounding_spheres].as_srv(D3D12_SHADER_VISIBILITY_ALL, 2);

            light_culling_root_signature = d3dx::d3d12_root_signature_desc{ &parameters[0], _countof(parameters) }.create();
            assert(light_culling_root_signature);
            NAME_D3D12_OBJECT(light_culling_root_signature, L"Light Culling Root Signature");

            // frustum grid pso
            {
                assert(!grid_frustum_pso);
                struct {
                    d3dx::d3d12_pipeline_state_subobject_root_signature root_signature{ light_culling_root_signature };
                    d3dx::d3d12_pipeline_state_subobject_cs cs{ shaders::get_engine_shader(shaders::engine_shader::grid_frustums_cs) };
                } stream;
                grid_frustum_pso = d3dx::create_pipeline_state(&stream, sizeof(stream));
                NAME_D3D12_OBJECT(grid_frustum_pso, L"Grid Frustum PSO");
                assert(grid_frustum_pso);
            }

            {
                // light culling pso
                assert(!light_culling_pso);
                struct {
                    d3dx::d3d12_pipeline_state_subobject_root_signature root_signature{ light_culling_root_signature };
                    d3dx::d3d12_pipeline_state_subobject_cs cs{ shaders::get_engine_shader(shaders::engine_shader::light_culling_cs) };
                } stream;
                light_culling_pso = d3dx::create_pipeline_state(&stream, sizeof(stream));
                NAME_D3D12_OBJECT(light_culling_pso, L"Light Culling PSO");
                assert(light_culling_pso);
            }
        }

        void resize_buffers(culling_parameters& culler)
        {
            const UINT frustum_count{ culler.frustum_count };

            const UINT frustums_buffer_size{ sizeof(hlsl::Frustum) * frustum_count };
            const UINT light_grid_buffer_size{ (UINT)math::align_size_up<sizeof(XMFLOAT4)>(sizeof(XMUINT2) * frustum_count) };
            const UINT light_index_list_buffer_size{ (UINT)math::align_size_up < sizeof(XMFLOAT4) >(sizeof(UINT) * max_lights_per_tile * frustum_count) };
            const UINT light_grid_and_index_list_buffer_size{ light_grid_buffer_size + light_index_list_buffer_size };

            resource::buffer_init_info info{};
            info.alignment = sizeof(XMFLOAT4);
            info.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            info.cpu_accessible = false;

            if (frustums_buffer_size > culler.frustums.size())
            {
                info.size = frustums_buffer_size;
                culler.frustums = resource::Buffer{ info };
                NAME_D3D12_OBJECT_INDEXED(culler.frustums.buffer(), frustum_count, L"Light Grid Frustums Buffer - count");
            }

            if (light_grid_and_index_list_buffer_size > culler.light_grid_and_index_list.size())
            {
                info.size = light_grid_and_index_list_buffer_size;
                culler.light_grid_and_index_list = resource::Buffer(info);

                const D3D12_GPU_VIRTUAL_ADDRESS light_grid_opaque_buffer{ culler.light_grid_and_index_list.gpu_address() };
                culler.light_index_list_opaque_buffer = light_grid_opaque_buffer + light_grid_buffer_size;
                NAME_D3D12_OBJECT_INDEXED(culler.light_grid_and_index_list.buffer(), light_grid_and_index_list_buffer_size, L"Light Grid and Index List Buffer - size");

                if (!culler.light_index_counter.buffer())
                {
                    info.size = 1;
                    info.alignment = sizeof(XMFLOAT4);
                    info.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
                    culler.light_index_counter = resource::uav_buffer{ info };
                    NAME_D3D12_OBJECT_INDEXED(culler.light_index_counter.buffer(), core::current_frame_index(), L"Light Index Counter Buffer");
                }
            }
        }

        void resize_and_calculate_grid_frustums(culling_parameters& culler,
            id3d12_graphics_command_list* cmd_list,
            const core::d3d12_frame_info d3d12_info,
            barriers::resource_barrier& barriers)
        {
            culler.camera_fov = d3d12_info.camera->field_of_view();
            culler.view_width = d3d12_info.surface_width;
            culler.view_height = d3d12_info.surface_height;

            //resize(culler);
            constexpr UINT tile_size{ light_culling_tile_size };
            assert(culler.view_width >= tile_size && culler.view_width >= tile_size);
            const XMUINT2 tile_count
            {
                (UINT)math::align_size_up<tile_size>(culler.view_width) / tile_size,
                (UINT)math::align_size_up<tile_size>(culler.view_height) / tile_size
            };

            culler.frustum_count = tile_count.x * tile_count.y;

            // Dispatch parameters for grid frustums
            {
                hlsl::LightCullingDispatchParameters& params{ culler.grid_frustums_dispatch_params };
                params.NumThreads = tile_count;
                params.NumThreadGroups.x = (UINT)math::align_size_up<tile_size>(tile_count.x) / tile_size;
                params.NumThreadGroups.y = (UINT)math::align_size_up<tile_size>(tile_count.y) / tile_size;
            }

            // Dispatch parameters for light culling
            {
                hlsl::LightCullingDispatchParameters& params{ culler.light_culling_dispatch_params };
                params.NumThreads.x = tile_count.x * tile_size;
                params.NumThreads.y = tile_count.y * tile_size;
                params.NumThreadGroups = tile_count;
            }

            resize_buffers(culler);

            //calculate_grid_frustums(culler, cmd_list, d3d12_info, barriers);
        }
    } // anonymous namespace

    UINT Light::entity_id() const
    {
        return m_id;
    }

    void create_light_set(UINT64 set_key)
    {
        // key not used check
        assert(!m_light_set_keys.count(set_key));
        m_light_set_keys[set_key] = {};
    }

    void remove_light_set(UINT64 light_set_key)
    {
        assert(!m_light_set_keys[light_set_key].has_lights());
        m_light_set_keys.erase(light_set_key);
    }

    void generate_lights()
    {
        create_light_set(light_set_states::left_set);
        create_light_set(light_set_states::right_set);

        light_init_info info{};
        // left

        // Directional light
        info.entity_id = app::create_entity_item({}, {}, { 1.f, 1.f, 1.f }, nullptr, nullptr).get_id();
        info.type = lights::light_type::directional;
        info.set_key = light_set_states::left_set;
        info.intensity = 1.f;
        info.color = rgb_to_color(174, 174, 174);
        lights.emplace_back(create_light(info));

        info.entity_id = app::create_entity_item({ 1.f, 1.f, 1.f }, { -math::pi * 0.5f, -math::pi * 0.5f, -math::pi * 0.5f }, { 1.f, 1.f, 1.f }, nullptr, nullptr).get_id();
        info.type = lights::light_type::spot;
        info.set_key = light_set_states::left_set;
        info.intensity = 1.f;
        info.color = rgb_to_color(174, 174, 174);
        lights.emplace_back(create_light(info));

        // right

        // Directional light
        info.entity_id = app::create_entity_item({}, {}, { 1.f, 1.f, 1.f }, nullptr, nullptr).get_id();
        info.type = lights::light_type::directional;
        info.set_key = light_set_states::right_set;
        info.intensity = 1.f;
        info.color = rgb_to_color(200, 200, 200);
        lights.emplace_back(create_light(info));

        info.entity_id = app::create_entity_item({ -1.f, -1.f, -1.f }, {}, { 1.f, 1.f, 1.f }, nullptr, nullptr).get_id();
        info.type = lights::light_type::point;
        info.set_key = light_set_states::right_set;
        info.intensity = 1.f;
        info.color = rgb_to_color(20, 200, 174);
        lights.emplace_back(create_light(info));
    }

    void remove_lights()
    {
        for (auto& light : lights)
        {
            const UINT id{ light.entity_id() };
            remove_light(light.get_id(), light.get_set_key());
            app::remove_game_entity(id);
        }

        for (auto& light : disabled_lights)
        {
            const UINT id{ light.entity_id() };
            remove_light(light.get_id(), light.get_set_key());
            app::remove_game_entity(id);
        }

        lights.clear();
        disabled_lights.clear();

        remove_light_set(light_set_states::left_set);
        remove_light_set(light_set_states::right_set);
    }

    UINT add_cull_light()
    {
        return light_cullers.add();
    }

    void remove_cull_light(UINT id)
    {
        assert(id != Invalid_Index);
        light_cullers.remove(id);
    }

    void update_light_buffers(core::d3d12_frame_info d3d12_info)
    {
        const UINT64 light_set_key{ d3d12_info.info->light_set_key };
        assert(m_light_set_keys.count(light_set_key));
        LightSet& light_set{ m_light_set_keys[light_set_key] };
        if (!light_set.has_lights()) return;

        light_set.update_transforms();
        const UINT frame_index{ d3d12_info.frame_index };
        LightBuffer& light_buffer{ light_buffers[frame_index] };
        light_buffer.update_light_buffer(light_set, light_set_key, frame_index);
    }

    void cull_lights(id3d12_graphics_command_list* cmd_list, core::d3d12_frame_info d3d12_info, barriers::resource_barrier& barriers)
    {
        const UINT id{ d3d12_info.light_id };
        assert(id != Invalid_Index);
        culling_parameters& culler{ light_cullers[id].cullers[d3d12_info.frame_index] };

        if (d3d12_info.surface_width != culler.view_width ||
            d3d12_info.surface_height != culler.view_height ||
            !math::is_equal(d3d12_info.camera->field_of_view(), culler.camera_fov))
        {
            resize_and_calculate_grid_frustums(culler, cmd_list, d3d12_info, barriers);
        }


        hlsl::LightCullingDispatchParameters& params{ culler.light_culling_dispatch_params };
        params.NumLights = m_light_set_keys[d3d12_info.info->light_set_key].cullable_light_count();
        params.DepthBufferSrvIndex = graphic_pass::get_depth_buffer().srv().index;

        // NOTE: we update culler.has_lights after this statement, so the light culling shader
        //       will run once to clear the buffers when there're no lights.
        if (!params.NumLights && !culler.has_lights) return;

        culler.has_lights = params.NumLights > 0;

        resource::constant_buffer& cbuffer{ core::cbuffer() };
        hlsl::LightCullingDispatchParameters* const buffer{ cbuffer.allocate<hlsl::LightCullingDispatchParameters>() };
        memcpy(buffer, &params, sizeof(hlsl::LightCullingDispatchParameters));

        // Make light grid and light index buffers writable
        barriers.add(culler.light_grid_and_index_list.buffer(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        barriers.apply(cmd_list);

        const XMUINT4 clear_value{ 0, 0, 0, 0 };
        culler.light_index_counter.clear_uav(cmd_list, &clear_value.x);

        cmd_list->SetComputeRootSignature(light_culling_root_signature);
        cmd_list->SetPipelineState(light_culling_pso);
        using param = light_culling_root_parameter;
        cmd_list->SetComputeRootConstantBufferView(param::global_shader_data, d3d12_info.global_shader_data);
        cmd_list->SetComputeRootConstantBufferView(param::constants, cbuffer.gpu_address(buffer));
        cmd_list->SetComputeRootUnorderedAccessView(param::frustums_out_or_index_counter, culler.light_index_counter.gpu_address());
        cmd_list->SetComputeRootShaderResourceView(param::frustums_in, culler.frustums.gpu_address());
        cmd_list->SetComputeRootShaderResourceView(param::culling_info, light_buffers[d3d12_info.frame_index].culling_info());
        cmd_list->SetComputeRootShaderResourceView(param::bounding_spheres, light_buffers[d3d12_info.frame_index].bounding_spheres());
        cmd_list->SetComputeRootUnorderedAccessView(param::light_grid_opaque, culler.light_grid_and_index_list.gpu_address());
        cmd_list->SetComputeRootUnorderedAccessView(param::light_index_list_opaque, culler.light_index_list_opaque_buffer);

        cmd_list->Dispatch(params.NumThreadGroups.x, params.NumThreadGroups.y, 1);

        // Make light grid and light index buffers readable
        // NOTE: this transition barrier will be applied by the caller of this function.
        barriers.add(culler.light_grid_and_index_list.buffer(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    }

    D3D12_GPU_VIRTUAL_ADDRESS non_cullable_light_buffer(UINT frame_index)
    {
        const LightBuffer& light_buffer{ light_buffers[frame_index] };
        return light_buffer.non_cullable_lights();
    }

    D3D12_GPU_VIRTUAL_ADDRESS cullable_light_buffer(UINT frame_index)
    {
        const LightBuffer& light_buffer{ light_buffers[frame_index] };
        return light_buffer.cullable_lights();
    }

    D3D12_GPU_VIRTUAL_ADDRESS culling_info_buffer(UINT frame_index)
    {
        const LightBuffer& light_buffer{ light_buffers[frame_index] };
        return light_buffer.culling_info();
    }

    D3D12_GPU_VIRTUAL_ADDRESS bounding_sphere_buffer(UINT frame_index)
    {
        const LightBuffer& light_buffer{ light_buffers[frame_index] };
        return light_buffer.bounding_spheres();
    }
    
    D3D12_GPU_VIRTUAL_ADDRESS frustums(UINT light_culling_id, UINT frame_index)
    {
        assert(frame_index < Frame_Count && light_culling_id != Invalid_Index);
        return light_cullers[light_culling_id].cullers[frame_index].frustums.gpu_address();
    }

    D3D12_GPU_VIRTUAL_ADDRESS light_grid_opaque(UINT light_culling_id, UINT frame_index)
    {
        assert(frame_index < Frame_Count && light_culling_id != Invalid_Index);

        //D3D12_GPU_VIRTUAL_ADDRESS temp = light_cullers[light_culling_id].cullers[frame_index].light_grid_and_index_list.gpu_address();
        return light_cullers[light_culling_id].cullers[frame_index].light_grid_and_index_list.gpu_address();
    }

    D3D12_GPU_VIRTUAL_ADDRESS light_index_list_opaque(UINT light_culling_id, UINT frame_index)
    {
        assert(frame_index < Frame_Count && light_culling_id != Invalid_Index);
        return light_cullers[light_culling_id].cullers[frame_index].light_index_list_opaque_buffer;
    }


    // D3D12LightCulling
    bool initialize()
    {
        create_root_signature_pso();
        return true;
    }

    // D3D12LightCulling
    void shutdown()
    {
        // D3D12Light.cpp
        assert(m_light_set_keys.empty());

        for (UINT i{ 0 }; i < Frame_Count; ++i)
        {
            light_buffers[i].release();
        }

        assert(light_culling_root_signature && grid_frustum_pso && light_culling_pso);
        core::deferred_release(light_culling_root_signature);
        core::deferred_release(grid_frustum_pso);
        core::deferred_release(light_culling_pso);
    }
}