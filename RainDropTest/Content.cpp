#include <unordered_map>
#include "Content.h"
#include "Math.h"
#include "Buffers.h"
#include "FreeList.h"
#include "Main.h"
#include "Helpers.h"
#include "GraphicPass.h"
#include "Core.h"
#include "Utilities.h"
#include "Upload.h"
//#include "ContentToEngine.h"
#include "Shaders.h"

//#include <iostream>
//#include <Windows.h>


namespace content
{
    using namespace DirectX;
    namespace {

        struct sub_mesh_view
        {
            D3D12_VERTEX_BUFFER_VIEW position_buffer_view{};
            D3D12_VERTEX_BUFFER_VIEW element_buffer_view{};
            D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
            D3D_PRIMITIVE_TOPOLOGY primitive_topology;
            UINT element_type{};
        };

        struct d3d12_render_item
        {
            UINT entity_id;
            UINT sub_mesh_gpu_id;
            UINT material_id;
            UINT graphic_pass_pso_id;
            UINT depth_pso_id;
        };

        // sub mesh

        utl::free_list<ID3D12Resource*> sub_mesh_buffers{ 1 };
        utl::free_list<sub_mesh_view> sub_mesh_views{ 2 };
        std::mutex sub_mesh_mutex{};

        // textures
        utl::free_list<resource::Texture_Buffer> textures{ 3 };
        utl::free_list<UINT> descriptor_indices{ 4 };
        std::mutex texture_mutex{};

        // material
        utl::vector<ID3D12RootSignature*> root_signatures;
        std::unordered_map<UINT64, UINT> material_root_signature_map;
        utl::free_list<std::unique_ptr<UINT8[]>> materials{ 5 };
        std::mutex material_mutex{};

        utl::free_list<d3d12_render_item> render_items{ 6 };
        utl::free_list<std::unique_ptr<UINT[]>>render_item_ids{ 7 };
        std::mutex render_item_mutex{};

        utl::vector<ID3D12PipelineState*> pipeline_states;
        std::unordered_map<UINT64, UINT> pso_map;
        std::mutex pso_mutex{};

        struct {
            utl::vector<level_of_detail_offset_count> lod_offsets_counts;
            utl::vector<UINT> geometry_ids;
        } frame_cache;

        // struct {
        //     material_type::type  type,
        //     shader_flags::flags  flags,
        //     id::id_type          root_signature_id,
        //     u32                  texture_count,
        //     material_surface     pbr_material_properties
        //     UINT                 shader_count;
        //     id::id_type          shader_ids[shader_count];
        //     id::id_type          texture_ids[texture_count],
        //     u32                  descriptor_indices[texture_count]
        // } material_buffer

        UINT create_root_signature(material_type::type type, shaders::shader_flags::flags flags);

        class Material_Stream
        {
        public:
            DISABLE_COPY_AND_MOVE(Material_Stream);
            explicit Material_Stream(UINT8* const material_buffer)
                : m_buffer{ material_buffer }
            {
                initialize();
            }

            // Create the new material buffer with the material init info
            explicit Material_Stream(std::unique_ptr<UINT8[]>& material_buffer, material_init_info info)
            {
                assert(!material_buffer);

                UINT shader_count{ 0 };
                UINT flags{ 0 };
                for (UINT i{ 0 }; i < shaders::shader_type::count; ++i)
                {
                    if (info.shader_ids[i] != Invalid_Index)
                    {
                        ++shader_count;
                        flags |= (1 << i);
                    }
                }

                assert(shader_count && flags);

                // Get the size of the new material buffer
                const UINT buffer_size{
                    sizeof(material_type::type) +           // material_type
                    sizeof(shaders::shader_flags::flags) +  // shader_flags
                    sizeof(UINT) +                          // root_signature_id
                    sizeof(UINT) +                          // texture_count
                    sizeof(material_surface) +              // pbr_material_properties
                    sizeof(UINT) * shader_count +           // shader ids
                    sizeof(UINT) * info.texture_count +     // texture_ids[texture_count],
                    sizeof(UINT) * info.texture_count       // descriptor_indices[texture_count]
                };

                // Make the material buffer
                material_buffer = std::make_unique<UINT8[]>(buffer_size);
                m_buffer = material_buffer.get();
                UINT8* const buffer{ m_buffer };

                UINT root_signature_id = create_root_signature(info.type, (shaders::shader_flags::flags)flags);

                // file the material buffer from material_file_buffer and the material_init_info
                *(material_type::type*)buffer = info.type;
                *(shaders::shader_flags::flags*)(&buffer[m_shader_flags_index]) = (shaders::shader_flags::flags)flags;
                *(UINT*)(&buffer[m_root_signature_index]) = root_signature_id;
                *(UINT*)(&buffer[m_texture_count_index]) = info.texture_count;
                *(material_surface*)(&buffer[m_material_surface_index]) = info.surface;

                initialize();

                if (info.texture_count)
                {
                    memcpy(m_texture_ids, info.texture_ids, info.texture_count * sizeof(UINT));
                    texture::get_descriptor_indices(m_texture_ids, info.texture_count, m_descriptor_indices);
                }

                UINT shader_index{ 0 };
                for (UINT i{ 0 }; i < shaders::shader_type::count; ++i)
                {
                    if (info.shader_ids[i] != Invalid_Index)
                    {
                        m_shader_ids[shader_index] = info.shader_ids[i];
                        ++shader_count;
                    }
                }

                assert(shader_index = (UINT)_mm_popcnt_u32(m_shader_flags));
            }

            [[nodiscard]] constexpr UINT texture_count() const { return m_texture_count; }
            [[nodiscard]] constexpr material_type::type material_type() const { return m_type; }
            [[nodiscard]] constexpr shaders::shader_flags::flags shader_flags() const { return m_shader_flags; }
            [[nodiscard]] constexpr UINT root_signature_id() const { return m_root_signature_id; }
            [[nodiscard]] constexpr UINT* texture_ids() const { return m_texture_ids; }
            [[nodiscard]] constexpr UINT* descriptor_indices() const { return m_descriptor_indices; }
            [[nodiscard]] constexpr UINT* shader_ids() const { return m_shader_ids; }
            [[nodiscard]] constexpr material_surface* surface() const { return m_material_surface; }

        private:
            void initialize()
            {
                // fill the class data from the material_buffer
                assert(m_buffer);
                UINT8* const buffer{ m_buffer };

                m_type = *(material_type::type*)buffer;
                m_shader_flags = *(shaders::shader_flags::flags*)(&buffer[m_shader_flags_index]);
                m_root_signature_id = *(UINT*)(&buffer[m_root_signature_index]);
                m_texture_count = *(UINT*)(&buffer[m_texture_count_index]);
                m_material_surface = (material_surface*)(&buffer[m_material_surface_index]);

                m_shader_ids = (UINT*)(&buffer[m_material_surface_index + sizeof(material_surface)]);
                m_texture_ids = m_texture_count ? &m_shader_ids[_mm_popcnt_u32(m_shader_flags)] : nullptr;
                m_descriptor_indices = m_texture_count ? (UINT*)(&m_texture_ids[m_texture_count]) : nullptr;
            }

            constexpr static UINT m_shader_flags_index{ sizeof(material_type::type) };
            constexpr static UINT m_root_signature_index{ m_shader_flags_index + sizeof(shaders::shader_flags::flags) };
            constexpr static UINT m_texture_count_index{ m_root_signature_index + sizeof(UINT) };
            constexpr static UINT m_material_surface_index{ m_texture_count_index + sizeof(UINT) };

            UINT8* m_buffer;
            material_surface* m_material_surface;
            UINT* m_texture_ids;
            UINT* m_descriptor_indices;
            UINT* m_shader_ids;
            UINT m_root_signature_id;
            UINT m_texture_count;
            material_type::type m_type;
            shaders::shader_flags::flags m_shader_flags;
        };

        constexpr D3D_PRIMITIVE_TOPOLOGY get_d3d_primitive_topology(primitive_topology::type type)
        {
            assert(type < primitive_topology::count);
            switch (type)
            {
            case primitive_topology::point_list: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
            case primitive_topology::line_list: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
            case primitive_topology::line_strip: return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
            case primitive_topology::triangle_list: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            case primitive_topology::triangle_strip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            }

            return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
        }

        constexpr D3D12_PRIMITIVE_TOPOLOGY_TYPE get_d3d_primitive_topology_type(D3D_PRIMITIVE_TOPOLOGY topology)
        {
            switch (topology)
            {
            case D3D_PRIMITIVE_TOPOLOGY_POINTLIST: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
            case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
            case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
            case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
            case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            }

            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
        }


        constexpr D3D12_ROOT_SIGNATURE_FLAGS
            get_root_signature_flags(shaders::shader_flags::flags flags)
        {
            D3D12_ROOT_SIGNATURE_FLAGS default_flags{ d3dx::d3d12_root_signature_desc::default_flags };
            if (flags & shaders::shader_flags::vertex)           default_flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
            if (flags & shaders::shader_flags::hull)             default_flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
            if (flags & shaders::shader_flags::domain)           default_flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
            if (flags & shaders::shader_flags::geometry)         default_flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
            if (flags & shaders::shader_flags::pixel)            default_flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
            if (flags & shaders::shader_flags::amplification)    default_flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS;
            if (flags & shaders::shader_flags::mesh)             default_flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;
            return default_flags;
        }

        UINT create_root_signature(material_type::type type, shaders::shader_flags::flags flags)
        {
            assert(type < material_type::count);
            const UINT64 key{ (UINT64)type << 32 | flags };
            auto pair = material_root_signature_map.find(key);
            if (pair != material_root_signature_map.end())
            {
                assert(pair->first == key);
                return pair->second;
            }

            ID3D12RootSignature* root_signature{ nullptr };

            switch (type)
            {
            case material_type::opaque:
            {
                using params = opaque_root_parameter;
                d3dx::d3d12_root_parameter parameters[params::count]{};

                D3D12_SHADER_VISIBILITY buffer_visibility{ D3D12_SHADER_VISIBILITY_VERTEX };
                D3D12_SHADER_VISIBILITY data_visibility{ D3D12_SHADER_VISIBILITY_ALL };

                parameters[params::global_shader_data].as_cbv(D3D12_SHADER_VISIBILITY_ALL, 0);
                parameters[params::per_object_data].as_cbv(data_visibility, 1);
                parameters[params::position_buffer].as_srv(buffer_visibility, 0);
                parameters[params::element_buffer].as_srv(buffer_visibility, 1);
                parameters[params::srv_indices].as_srv(D3D12_SHADER_VISIBILITY_PIXEL, 2); // TODO: needs to be visible to any stages that need to sample textures.
                parameters[params::directional_lights].as_srv(D3D12_SHADER_VISIBILITY_PIXEL, 3);
                parameters[params::cullable_lights].as_srv(D3D12_SHADER_VISIBILITY_PIXEL, 4);
                parameters[params::light_grid].as_srv(D3D12_SHADER_VISIBILITY_PIXEL, 5);
                parameters[params::light_index_list].as_srv(D3D12_SHADER_VISIBILITY_PIXEL, 6);

                const D3D12_STATIC_SAMPLER_DESC samplers[]
                {
                    d3dx::static_sampler(d3dx::sampler_state.static_point, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL),
                    d3dx::static_sampler(d3dx::sampler_state.static_linear, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL),
                    d3dx::static_sampler(d3dx::sampler_state.static_anisotropic, 2, 0, D3D12_SHADER_VISIBILITY_PIXEL),
                };

                D3D12_ROOT_SIGNATURE_FLAGS flags{ d3dx::d3d12_root_signature_desc::default_flags };
                flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
                flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

                root_signature = d3dx::d3d12_root_signature_desc
                {
                    &parameters[0],
                    _countof(parameters),
                    flags,
                    &samplers[0], _countof(samplers)
                }.create();
            }
            break;
            }

            assert(root_signature);
            const UINT id{ (UINT)root_signatures.size() };
            root_signatures.emplace_back(root_signature);
            material_root_signature_map[key] = id;
            NAME_D3D12_OBJECT_INDEXED(root_signature, key, L"Main Root Signature - key");

            return id;
        }

        UINT create_pso_if_needed(const UINT8* const stream_ptr, UINT64 aligned_stream_size)
        {
            const UINT64 key{ math::calc_crc32_u64(stream_ptr, aligned_stream_size) };

            // Lock scope to check if PSO already exists
            {
                std::lock_guard lock{ pso_mutex };
                auto pair = pso_map.find(key);

                if (pair != pso_map.end())
                {
                    assert(pair->first == key);
                    return pair->second;
                }
            }

            // Creating a new PSO is lock-free
            d3dx::d3d12_pipeline_state_subobject_stream* const stream{ (d3dx::d3d12_pipeline_state_subobject_stream* const)stream_ptr };
            ID3D12PipelineState* pso{ d3dx::create_pipeline_state(stream, sizeof(d3dx::d3d12_pipeline_state_subobject_stream)) };

            // Lock scope to add the new PSO's pointer and id (I know, scoping is not necessary, but it's more obvious this way.)
            {
                std::lock_guard lock{ pso_mutex };
                const UINT id{ (UINT)pipeline_states.size() };
                pipeline_states.emplace_back(pso);
                NAME_D3D12_OBJECT_INDEXED(pipeline_states.back(), key, L"Pipeline State Object - key");

                pso_map[key] = id;
                return id;
            }
        }

        UINT create_graphic_pso(UINT material_id, D3D12_PRIMITIVE_TOPOLOGY primitive_topology, UINT elements_type)
        {
            constexpr UINT64 aligned_stream_size{ math::align_size_up<sizeof(UINT64)>(sizeof(d3dx::d3d12_pipeline_state_subobject_stream)) };
            UINT8* const stream_ptr{ (UINT8* const)_malloca(aligned_stream_size) };
            ZeroMemory(stream_ptr, aligned_stream_size);
            new (stream_ptr) d3dx::d3d12_pipeline_state_subobject_stream{};

            d3dx::d3d12_pipeline_state_subobject_stream& stream{ *(d3dx::d3d12_pipeline_state_subobject_stream* const)stream_ptr };

            // lock materials
            {
                std::lock_guard lock{ material_mutex };

                const  Material_Stream material{ materials[material_id].get() };
                const UINT8* const material_ptr = (UINT8* const)materials[material_id].get();

                D3D12_RT_FORMAT_ARRAY rt_array{};
                rt_array.NumRenderTargets = 1;
                rt_array.RTFormats[0] = graphic_pass::main_buffer_format;

                stream.render_target_formats = rt_array;
                stream.root_signature = root_signatures[material.root_signature_id()];
                stream.primitive_topology = get_d3d_primitive_topology_type(primitive_topology);
                stream.depth_stencil_format = graphic_pass::depth_buffer_format;
                stream.rasterizer = d3dx::rasterizer_state.face_cull;
                stream.depth_stencil1 = d3dx::depth_state.reversed_readonly;
                stream.blend = d3dx::blend_state.disabled;

                const shaders::shader_flags::flags flags{ material.shader_flags() };
                const UINT key{ shaders::element_type_to_shader_id(elements_type) };
                stream.vs = shaders::get_engine_shader(key);
                //stream.ps = shaders::get_engine_shader(shaders::engine_shader::pixel_shader_ps);
                stream.ps = shaders::get_engine_shader(shaders::engine_shader::texture_shader_ps);
            }

            UINT pso_id = create_pso_if_needed(stream_ptr, aligned_stream_size);

            //stream.depth_stencil1 = d3dx::depth_state.reversed;
            //const UINT key{ shaders::element_type_to_shader_id(elements_type) };

            _freea(stream_ptr);
            return pso_id;

        }

        UINT create_depth_pso(UINT material_id, D3D12_PRIMITIVE_TOPOLOGY primitive_topology, UINT elements_type)
        {
            constexpr UINT64 aligned_stream_size{ math::align_size_up<sizeof(UINT64)>(sizeof(d3dx::d3d12_pipeline_state_subobject_stream)) };
            UINT8* const stream_ptr{ (UINT8* const)_malloca(aligned_stream_size) };
            ZeroMemory(stream_ptr, aligned_stream_size);
            new (stream_ptr) d3dx::d3d12_pipeline_state_subobject_stream{};

            d3dx::d3d12_pipeline_state_subobject_stream& stream{ *(d3dx::d3d12_pipeline_state_subobject_stream* const)stream_ptr };

            // lock materials
            {
                std::lock_guard lock{ material_mutex };

                const  Material_Stream material{ materials[material_id].get() };
                const UINT8* const material_ptr = (UINT8* const)materials[material_id].get();

                stream.root_signature = root_signatures[material.root_signature_id()];

                const shaders::shader_flags::flags flags{ material.shader_flags() };
                const UINT key{ shaders::element_type_to_shader_id(elements_type) };
                stream.vs = shaders::get_engine_shader(key);
            }

            D3D12_RT_FORMAT_ARRAY rt_array{};
            rt_array.NumRenderTargets = 1;
            rt_array.RTFormats[0] = graphic_pass::main_buffer_format;

            stream.render_target_formats = rt_array;
            stream.primitive_topology = get_d3d_primitive_topology_type(primitive_topology);
            stream.depth_stencil_format = graphic_pass::depth_buffer_format;
            stream.rasterizer = d3dx::rasterizer_state.face_cull;
            stream.depth_stencil1 = d3dx::depth_state.reversed_readonly;
            stream.blend = d3dx::blend_state.disabled;

            stream.depth_stencil1 = d3dx::depth_state.reversed;
            UINT pso_id = create_pso_if_needed(stream_ptr, aligned_stream_size);

            _freea(stream_ptr);
            return pso_id;
        }

        resource::Texture_Buffer create_resource_from_texture_data(const UINT8* const data)
        {
            // struct {
            //     u32 width, height, array_size (or depth), flags, mip_levels, format,
            //     struct {
            //         u32 row_pitch, slice_pitch,
            //         u8 image[mip_level][slice_pitch * depth_per_mip],
            //     } images[]
            // } texture

            assert(data);
            utl::blob_stream_reader blob{ data };
            const UINT width{ blob.read<UINT>() };
            const UINT height{ blob.read<UINT>() };
            UINT depth{ 1 };
            UINT array_size{ blob.read<UINT>() };
            const UINT flags{ blob.read<UINT>() };
            const UINT mip_levels{ blob.read<UINT>() };
            const DXGI_FORMAT format{ (DXGI_FORMAT)blob.read<UINT>() };
            const bool is_3d{ (flags & content::texture_flags::is_volume_map) != 0 };

            assert(mip_levels <= resource::Texture_Buffer::max_mips);

            UINT depth_per_mip_level[resource::Texture_Buffer::max_mips]{};
            for (UINT i{ 0 }; i < resource::Texture_Buffer::max_mips; ++i)
            {
                depth_per_mip_level[i] = 1;
            }

            if (is_3d)
            {
                depth = array_size;
                array_size = 1;
                UINT depth_per_mip{ depth };

                for (UINT i{ 0 }; i < mip_levels; ++i)
                {
                    depth_per_mip_level[i] = depth_per_mip;
                    depth_per_mip = (depth_per_mip >> 1, (UINT)1) ? depth_per_mip >> 1 : (UINT)1;
                }
            }

            // set the D3D12_SUBRESOURCE_DATA
            utl::vector<D3D12_SUBRESOURCE_DATA> subresources{};

            for (UINT i{ 0 }; i < array_size; ++i)
            {
                for (UINT j{ 0 }; j < mip_levels; ++j)
                {
                    const UINT row_pitch{ blob.read<UINT>() };
                    const UINT slice_pitch{ blob.read<UINT>() };

                    subresources.emplace_back(D3D12_SUBRESOURCE_DATA
                        {
                            blob.position(),
                            row_pitch,
                            slice_pitch
                        });

                    // skip the rest of slices.
                    blob.skip(slice_pitch * depth_per_mip_level[j]);
                }
            }

            // set the GetCopyableFootprints
            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = is_3d ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;  //  D3D12_RESOURCE_DIMENSION Dimension;
            desc.Alignment = 0;                                                  //  UINT64 Alignment;
            desc.Width = width;                                                  //  UINT64 Width;
            desc.Height = height;                                                //  UINT Height;
            desc.DepthOrArraySize = is_3d ? (UINT16)depth : (UINT16)array_size; //  UINT16 DepthOrArraySize;
            desc.MipLevels = (UINT16)mip_levels;                                 //  UINT16 MipLevels;
            desc.Format = format;                                                //  DXGI_FORMAT Format;
            desc.SampleDesc = { 1, 0 };                                          //  DXGI_SAMPLE_DESC SampleDesc;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;                          //  D3D12_TEXTURE_LAYOUT Layout;
            desc.Flags = D3D12_RESOURCE_FLAG_NONE;                               //  D3D12_RESOURCE_FLAGS Flags;

            assert(!(flags & content::texture_flags::is_cube_map && (array_size % 6)));
            const UINT subresource_count{ array_size * mip_levels };
            assert(subresource_count);

            // struct footprints_data
            // {
            //     D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint[subresource_count];
            //     UINT num_rows[subresource_count];
            //     UINT64 row_sizes[subresource_count];
            // }
            const UINT footprints_data_size{ (sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) + sizeof(UINT) + sizeof(UINT64)) * subresource_count };
            std::unique_ptr<UINT8[]> footprints_data{ std::make_unique<UINT8[]>(footprints_data_size) };

            D3D12_PLACED_SUBRESOURCE_FOOTPRINT* const footprints_data_layouts{ (D3D12_PLACED_SUBRESOURCE_FOOTPRINT* const)footprints_data.get() };
            UINT* const num_rows{ (UINT* const)&footprints_data_layouts[subresource_count] };
            UINT64* const row_sizes{ (UINT64* const)&num_rows[subresource_count] };
            UINT64 required_size{ 0 };

            core::device()->GetCopyableFootprints(&desc, 0, subresource_count, 0, footprints_data_layouts, num_rows, row_sizes, &required_size);
            assert(required_size);

            // put the data in the gpu
            upload::Upload_Context context{ (UINT)required_size };
            UINT8* const cpu_address{ (UINT8* const)context.cpu_address() };

            // Copy each of the subresource to CPU
            for (UINT subresource_index{ 0 }; subresource_index < subresource_count; ++subresource_index)
            {
                const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint_layout{ footprints_data_layouts[subresource_index] };
                const UINT rows{ num_rows[subresource_index] };
                const UINT depth{ footprint_layout.Footprint.Depth };
                const D3D12_SUBRESOURCE_DATA& subresource{ subresources[subresource_index] };

                const D3D12_MEMCPY_DEST copy_dst
                {
                    cpu_address + footprint_layout.Offset,
                    footprint_layout.Footprint.RowPitch,
                    footprint_layout.Footprint.RowPitch * height
                };

                for (UINT depth_index{ 0 }; depth_index < depth; ++depth_index)
                {
                    UINT8* const src_slice{ (UINT8* const)subresource.pData + subresource.SlicePitch * depth_index };
                    UINT8* const dst_slice{ (UINT8* const)copy_dst.pData + copy_dst.SlicePitch * depth_index };

                    for (UINT row_index{ 0 }; row_index < rows; ++row_index)
                    {
                        size_t size = row_sizes[subresource_index];
                        memcpy(dst_slice + copy_dst.RowPitch * row_index, src_slice + subresource.RowPitch * row_index, row_sizes[subresource_index]);
                    }
                }
            }

            // Create the default buffer on the gpu for the data
            ID3D12Resource* resource{ nullptr };
            ThrowIfFailed(core::device()->CreateCommittedResource(&d3dx::heap_properties.default_heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource)));
            ID3D12Resource* upload_buffer{ context.upload_buffer() };

            for (UINT i{ 0 }; i < subresource_count; ++i)
            {
                D3D12_TEXTURE_COPY_LOCATION src
                {
                    upload_buffer,
                    D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
                    footprints_data_layouts[i]
                };

                D3D12_TEXTURE_COPY_LOCATION dst
                {
                    resource,
                    D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                    i
                };
                context.command_list()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            }

            context.end_upload();

            // Create the shader view
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
            resource::texture_init_info info{};
            info.resource = resource;

            if (flags & content::texture_flags::is_cube_map)
            {
                assert(array_size % 6 == 0);
                srv_desc.Format = format;
                srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

                if (array_size > 6)
                {
                    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
                    srv_desc.TextureCubeArray.MostDetailedMip = 0;
                    srv_desc.TextureCubeArray.MipLevels = mip_levels;
                    srv_desc.TextureCubeArray.NumCubes = array_size / 6;
                }
                else
                {
                    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                    srv_desc.TextureCubeArray.MostDetailedMip = 0;
                    srv_desc.TextureCubeArray.MipLevels = mip_levels;
                    srv_desc.TextureCubeArray.ResourceMinLODClamp = 0.f;
                }

                info.srv_desc = &srv_desc;
            }

            return resource::Texture_Buffer{ info };
        }

        // ContentToEngine.cpp

        constexpr uintptr_t single_mesh_marker{ (uintptr_t)0x01 };
        utl::free_list<UINT8*> geometry_hierarchies{ 8 };
        std::mutex geometry_mutex;

        UINT get_geometry_hierarchy_buffer_size(const void* const data)
        {
            const UINT8* data_ptr{ (UINT8*)data };
            const geometry_data* geometry_ptr = (geometry_data*)(data);
            const UINT level_of_detail_count = geometry_ptr->level_of_detail_count;

            UINT size{ sizeof(UINT) + (sizeof(float) + sizeof(level_of_detail_offset_count) * level_of_detail_count) };
            // Get the gpu_ids for the total_number_of_sub_meshes
            UINT64 offset = sizeof(UINT);
            for (UINT i{ 0 }; i < level_of_detail_count; ++i)
            {
                geometry_header* mesh_ptr = (geometry_header*)&data_ptr[offset];
                size += sizeof(UINT) * mesh_ptr->sub_mesh_count;
                offset += sizeof(geometry_header) + mesh_ptr->size_of_sub_meshes;
            }

            return size;
        }

        UINT create_material_resource(const void* const data)
        {
            assert(data);
            return content::material::add(*(const content::material_init_info* const)data);
        }

        UINT create_single_sub_mesh(const void* const data)
        {
            const UINT8* ptr = (UINT8*)(data)+sizeof(geometry_data);
            const UINT gpu_id{ sub_mesh::add(ptr) };

            // Create a fake pointer and it in the hierarchies.
            constexpr UINT8 shift_bits{ (sizeof(uintptr_t) - sizeof(UINT)) << 3 };
            UINT8* const fake_pointer{ (UINT8* const)((((uintptr_t)gpu_id) << shift_bits) | single_mesh_marker) };
            std::lock_guard lock{ geometry_mutex };

            return geometry_hierarchies.add(fake_pointer);
        }

        // struct{
        //     u32 lod_count,
        //     struct {
        //         f32 lod_threshold,
        //         u32 submesh_count,
        //         u32 size_of_submeshes,
        //         struct {
        //             u32 element_size, u32 vertex_count,
        //             u32 index_count, u32 elements_type, u32 primitive_topology
        //             u8 positions[sizeof(f32) * 3 * vertex_count],     // sizeof(positions) must be a multiple of 4 bytes. Pad if needed.
        //             u8 elements[sizeof(element_size) * vertex_count], // sizeof(elements) must be a multiple of 4 bytes. Pad if needed.
        //             u8 indices[index_size * index_count]
        //         } submeshes[sub_mesh_count]
        //     } mesh_lods[lod_count]
        // } geometry;
        //
        // Output format
        // If geometry has more than one LOD or sub_mesh:
        // struct {
        //     u32 lod_count,
        //     f32 thresholds[lod_count]
        //     struct {
        //         u16 offset,
        //         u16 count
        //     } lod_offsets[lod_count],
        //     id::id_type gpu_ids[total_number_of_sub_meshes]
        // } geometry_hierarchy
        UINT create_mesh_hierarchy(const void* const data)
        {
            const UINT size{ get_geometry_hierarchy_buffer_size(data) };
            UINT8* const hierarchy_buffer{ (UINT8* const)malloc(size) };
            assert(hierarchy_buffer);

            struct geometry_data* ptr = (geometry_data*)data;
            const UINT level_of_detail_count{ ptr->level_of_detail_count };
            assert(level_of_detail_count);

            *(UINT*)hierarchy_buffer = level_of_detail_count;

            float* const thresholds{ (float*)&hierarchy_buffer[sizeof(UINT)] };
            level_of_detail_offset_count* lod_offset_count{ (level_of_detail_offset_count*)&hierarchy_buffer[sizeof(UINT) + (sizeof(float) * level_of_detail_count)] };
            UINT* const gpu_ids{ (UINT*)&hierarchy_buffer[sizeof(UINT) + ((sizeof(float) + sizeof(level_of_detail_offset_count)) * level_of_detail_count)] };

            UINT sub_mesh_index{ 0 };
            for (UINT level_of_detail_idx{ 0 }; level_of_detail_idx < level_of_detail_count; ++level_of_detail_idx)
            {
                thresholds[level_of_detail_idx] = ptr->header.level_of_detail_threshold;
                const UINT sub_mesh_count = ptr->header.sub_mesh_count;
                assert(sub_mesh_count);
                lod_offset_count[level_of_detail_idx].count = sub_mesh_count;
                lod_offset_count[level_of_detail_idx].offset = sub_mesh_index;
                const UINT8* sub_mesh_ptr = (UINT8*)data + sizeof(geometry_data);
                for (UINT id_idx{ 0 }; id_idx < sub_mesh_count; ++id_idx)
                {
                    gpu_ids[sub_mesh_index] = sub_mesh::add(sub_mesh_ptr);
                    ++sub_mesh_index;

                    sub_mesh_ptr += ptr->header.size_of_sub_meshes;
                }
            }

            // Check the thresholds are increasing
            assert([&]() {
                float previous_threshold{ thresholds[0] };
                for (UINT i{ 0 }; i < level_of_detail_count; ++i)
                {
                    if (thresholds[i] < previous_threshold) return false;
                    previous_threshold = thresholds[i];
                }
                return true;
                }());
            return geometry_hierarchies.add(hierarchy_buffer);
        }

        bool is_single_mesh(const void* const data)
        {
            struct geometry_data* ptr = (geometry_data*)data;
            assert(ptr->level_of_detail_count);
            if (ptr->level_of_detail_count > 1) return false;

            assert(ptr->header.sub_mesh_count);
            return ptr->header.sub_mesh_count == 1;
        }

        constexpr UINT gpu_id_from_fake_pointer(UINT8* const pointer)
        {
            assert((uintptr_t)pointer & single_mesh_marker);
            constexpr UINT8 shift_bits{ (sizeof(uintptr_t) - sizeof(UINT)) << 3 };
            return (((uintptr_t)pointer) >> shift_bits) & (uintptr_t)Invalid_Index;
        }

        // NOTE: Expects 'data' to contain:
        // struct{
        //     u32 lod_count,
        //     struct {
        //         f32 lod_threshold,
        //         u32 sub_mesh_count,
        //         u32 size_of_sub_meshes,
        //         struct {
        //             u32 element_size, u32 vertex_count,
        //             u32 index_count, u32 elements_type, u32 primitive_topology
        //             u8 positions[sizeof(f32) * 3 * vertex_count],     // sizeof(positions) must be a multiple of 4 bytes. Pad if needed.
        //             u8 elements[sizeof(element_size) * vertex_count], // sizeof(elements) must be a multiple of 4 bytes. Pad if needed.
        //             u8 indices[index_size * index_count]
        //         } sub_meshes[sub_mesh_count]
        //     } mesh_level_of_details[lod_count]
        // } geometry;
        //
        // Output format
        //
        // If geometry has more than one LOD or sub_mesh:
        // struct {
        //     u32 lod_count,
        //     f32 thresholds[lod_count]
        //     struct {
        //         u16 offset,
        //         u16 count
        //     } lod_offsets[lod_count],
        //     id::id_type gpu_ids[total_number_of_sub_meshes]
        // } geometry_hierarchy
        // 
        // If geometry has a single LOD and sub_mesh:
        //
        // (gpu_id << 32) | 0x01
        //
        UINT create_geometry_resource(const void* const data)
        {
            return is_single_mesh(data) ? create_single_sub_mesh(data) : create_mesh_hierarchy(data);
        }

        UINT create_texture_resource(const void* const data)
        {
            assert(data);
            return content::texture::add((const UINT8* const)data);
        }

        void destroy_material_resource(UINT id)
        {
            content::material::remove(id);
        }

        // If geometry has more than one LOD or sub_mesh:
        // struct {
        //     u32 lod_count,
        //     f32 thresholds[lod_count]
        //     struct {
        //         u16 offset,
        //         u16 count
        //     } lod_offsets[lod_count],
        //     id::id_type gpu_ids[total_number_of_sub_meshes]
        // } geometry_hierarchy
        void destroy_geometry_resource(UINT id)
        {
            std::lock_guard lock{ geometry_mutex };
            UINT8* const pointer{ geometry_hierarchies[id] };
            if ((uintptr_t)pointer & single_mesh_marker)
            {
                content::sub_mesh::remove(gpu_id_from_fake_pointer(pointer));
            }
            else
            {
                struct geometry_data* ptr = (geometry_data*)pointer;
                const UINT level_of_detail_count{ ptr->level_of_detail_count };
                assert(level_of_detail_count);
                level_of_detail_offset_count* lod_offset_count{ (level_of_detail_offset_count*)&pointer[sizeof(UINT) + (sizeof(float) * level_of_detail_count)] };
                UINT* const gpu_ids{ (UINT*)&pointer[sizeof(UINT) + ((sizeof(float) + sizeof(level_of_detail_offset_count)) * level_of_detail_count)] };
                UINT id_index{ 0 };
                for (UINT lod_idx{ 0 }; lod_idx < level_of_detail_count; ++lod_idx)
                {
                    for (UINT i{ 0 }; i < lod_offset_count[lod_idx].count; ++i)
                    {
                        content::sub_mesh::remove(gpu_ids[id_index]);
                        ++id_index;
                    }
                }

                free(pointer);
            }
            geometry_hierarchies.remove(id);
        }

        void destroy_texture_resource(UINT id)
        {
            texture::remove(id);
        }

        UINT lod_from_threshold(float threshold, float* thresholds, UINT lod_count)
        {
            assert(threshold >= 0);
            if (lod_count == 1)
            {
                return 0;
            }

            for (UINT i{ lod_count - 1 }; i > 0; --i)
            {
                if (thresholds[i] <= threshold)
                {
                    return i;
                }
            }

            return 0;
        }

    } // anonymous namespace

    namespace sub_mesh {

        // NOTE: Expects 'data' to contain:
        // 
        //     u32 element_size, u32 vertex_count,
        //     u32 index_count, u32 elements_type, u32 primitive_topology
        //     u8 positions[sizeof(f32) * 3 * vertex_count],     // sizeof(positions) must be a multiple of 4 bytes. Pad if needed.
        //     u8 elements[sizeof(element_size) * vertex_count], // sizeof(elements) must be a multiple of 4 bytes. Pad if needed.
        //     u8 indices[index_size * index_count],
        //
        // Remarks:
        // - Advances the data pointer
        // - Position and element buffers should be padded to be a multiple of 4 bytes in length.
        //   This 16 bytes is defined as D3D12_STANDARD_MAXIMUM_ELEMENT_ALIGNMENT_BYTE_MULTIPLE.
        UINT add(const UINT8*& data)
        {
            assert(data);
            geometry_sub_mesh_header* header = (geometry_sub_mesh_header*)data;
            const UINT element_size{ header->element_size };
            const UINT vertex_count{ header->vertex_count };
            const UINT index_count{ header->index_count };
            const UINT element_type{ header->elements_type };
            const UINT primitive_topology{ header->primitive_topology };

            const UINT index_size{ (vertex_count < (1 << 16)) ? sizeof(UINT16) : sizeof(UINT) };

            // Note: element size may be 0, for position-only vertex formats.
            constexpr UINT alignment{ D3D12_STANDARD_MAXIMUM_ELEMENT_ALIGNMENT_BYTE_MULTIPLE };
            const UINT position_buffer_size{ sizeof(XMFLOAT3) * vertex_count };
            const UINT element_buffer_size{ element_size * vertex_count };
            const UINT index_buffer_size{ index_size * index_count };

            const UINT aligned_position_buffer_size{ (UINT)math::align_size_up<alignment>(position_buffer_size) };
            const UINT aligned_element_buffer_size{ (UINT)math::align_size_up<alignment>(element_buffer_size) };
            const UINT total_buffer_size{ aligned_position_buffer_size + aligned_element_buffer_size + index_buffer_size };

            ID3D12Resource* resource{ buffers::create_buffer_default_with_upload((const void*)&data[sizeof(geometry_sub_mesh_header)], total_buffer_size) };
            data += total_buffer_size;

            sub_mesh_view view{};
            view.position_buffer_view.BufferLocation = resource->GetGPUVirtualAddress();
            view.position_buffer_view.SizeInBytes = position_buffer_size;
            view.position_buffer_view.StrideInBytes = sizeof(XMFLOAT3);

            if (element_size)
            {
                view.element_buffer_view.BufferLocation = resource->GetGPUVirtualAddress() + aligned_position_buffer_size;
                view.element_buffer_view.SizeInBytes = element_buffer_size;
                view.element_buffer_view.StrideInBytes = element_size;
            }

            view.index_buffer_view.BufferLocation = resource->GetGPUVirtualAddress() + aligned_position_buffer_size + aligned_element_buffer_size;
            view.index_buffer_view.SizeInBytes = index_buffer_size;
            view.index_buffer_view.Format = (index_size == sizeof(UINT16)) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

            view.primitive_topology = get_d3d_primitive_topology((primitive_topology::type)primitive_topology);
            view.element_type = element_type;

            std::lock_guard lock{ sub_mesh_mutex };
            sub_mesh_buffers.add(resource);
            return sub_mesh_views.add(view);
        }

        void remove(UINT id)
        {
            std::lock_guard lock{ sub_mesh_mutex };
            sub_mesh_views.remove(id);

            core::deferred_release(sub_mesh_buffers[id]);
            sub_mesh_buffers.remove(id);
        }

        D3D_PRIMITIVE_TOPOLOGY get_primitive_topology(UINT id)
        {
            return sub_mesh_views[id].primitive_topology;
        }

        UINT get_views_element_type(UINT id)
        {
            return sub_mesh_views[id].element_type;
        }

        void get_views(const UINT id_count, const graphic_pass::graphic_cache& cache)
        {
            assert(cache.sub_mesh_gpu_ids && id_count);
            assert(cache.position_buffers && cache.element_buffers && cache.index_buffer_views &&
                cache.primitive_topologies && cache.elements_types);

            std::lock_guard lock{ sub_mesh_mutex };
            for (UINT i{ 0 }; i < id_count; ++i)
            {
                const sub_mesh_view& view{ sub_mesh_views[cache.sub_mesh_gpu_ids[i]] };
                cache.position_buffers[i] = view.position_buffer_view.BufferLocation;
                cache.element_buffers[i] = view.element_buffer_view.BufferLocation;
                cache.index_buffer_views[i] = view.index_buffer_view;
                cache.primitive_topologies[i] = view.primitive_topology;
                cache.elements_types[i] = view.element_type;
            }
        }
    } // namespace sub_mesh

    namespace texture {
        // NOTE: expects data to contain
        // struct {
        //     u32 width, height, array_size (or depth), flags, mip_levels, format,
        //     struct {
        //         u32 row_pitch, slice_pitch,
        //         u8 image[mip_level][slice_pitch * depth_per_mip],
        //     } images[]
        // } texture
        UINT add(const UINT8* const data)
        {
            assert(data);
            resource::Texture_Buffer texture{ create_resource_from_texture_data(data) };

            std::lock_guard lock{ texture_mutex };
            const UINT id{ textures.add(std::move(texture)) };
            descriptor_indices.add(textures[id].srv().index);
            return id;
        }

        void remove(UINT id)
        {
            std::lock_guard lock{ texture_mutex };
            textures.remove(id);
            descriptor_indices.remove(id);
        }

        void get_descriptor_indices(const UINT* const texture_ids, UINT id_count, UINT* const indices)
        {
            assert(texture_ids && id_count && indices);
            std::lock_guard lock{ texture_mutex };

            for (UINT i{ 0 }; i < id_count; ++i)
            {
                assert(texture_ids[i] != Invalid_Index);
                indices[i] = descriptor_indices[texture_ids[i]];
            }
        }
    } // namespace texture

    namespace material {
        // Output format:
        //
        // struct {
        // material_type::type  type,
        // shader_flags::flags  flags,
        // id::id_type          root_signature_id,
        // u32                  texture_count,
        // id::id_type          shader_ids[shader_count],
        // id::id_type          texture_ids[texture_count],
        // u32*                 descriptor_indices[texture_count]
        // } d3d12_material
        UINT add(content::material_init_info info)
        {
            std::unique_ptr<UINT8[]> buffer;
            std::lock_guard lock{ material_mutex };
            Material_Stream stream{ buffer, info };
            assert(buffer);
            return materials.add(std::move(buffer));
        }

        void remove(UINT id)
        {
            std::lock_guard lock{ material_mutex };
            materials.remove(id);
        }

        //void get_materials(const UINT* const material_ids, UINT material_count, const content::material::materials_cache& cache, UINT descriptor_index_count)
        //{
        //    assert(material_ids && material_count);
        //    assert(cache.root_signatures && cache.material_types);
        //    std::lock_guard lock{ material_mutex };

        //    u32 total_index_count{ 0 };

        //    for (u32 i{ 0 }; i < material_count; ++i)
        //    {
        //        const d3d12_material_stream stream{ materials[material_ids[i]].get() };
        //        cache.root_signatures[i] = root_signatures[stream.root_signature_id()];
        //        cache.material_types[i] = stream.material_type();
        //        cache.descriptor_indices[i] = stream.descriptor_indices();
        //        cache.texture_count[i] = stream.texture_count();
        //        total_index_count += stream.texture_count();
        //    }

        //    descriptor_index_count = total_index_count;
        //}

        void get_materials(UINT id_count, graphic_pass::graphic_cache& cache)
            //void get_materials(const UINT* const material_ids, UINT material_count, const graphic_pass::graphic_cache& cache, UINT& descriptor_index_count)
        {
            assert(cache.material_ids);
            assert(cache.root_signatures && cache.material_types);

            std::lock_guard lock{ material_mutex };

            UINT total_index_count{ 0 };

            for (UINT i{ 0 }; i < id_count; ++i)
            {
                const Material_Stream stream{ materials[cache.material_ids[i]].get() };

                cache.root_signatures[i] = root_signatures[stream.root_signature_id()];
                cache.material_types[i] = stream.material_type();
                cache.descriptor_indices[i] = stream.descriptor_indices();
                cache.texture_counts[i] = stream.texture_count();
                cache.material_surfaces[i] = stream.surface();
                total_index_count += stream.texture_count();
            }

            cache.descriptor_index_count = total_index_count;
        }

    } // namespace material

    namespace render_item {

        UINT add(UINT entity_id, UINT geometry_content_id, UINT material_count, const UINT* const material_ids)
        {
            assert(entity_id != Invalid_Index && geometry_content_id != Invalid_Index);
            assert(material_count && material_ids);

            UINT* const gpu_ids{ (UINT* const)_malloca(material_count * sizeof(UINT)) };
            content::get_sub_mesh_gpu_ids(geometry_content_id, material_count, gpu_ids);

            //sub_mesh::  views_cache
            //{
            //    (D3D12_GPU_VIRTUAL_ADDRESS* const)_malloca(material_count * sizeof(D3D12_GPU_VIRTUAL_ADDRESS)),
            //    (D3D12_GPU_VIRTUAL_ADDRESS* const)_malloca(material_count * sizeof(D3D12_GPU_VIRTUAL_ADDRESS)),
            //    (D3D12_INDEX_BUFFER_VIEW* const)_malloca(material_count * sizeof(D3D12_INDEX_BUFFER_VIEW)),
            //    (D3D_PRIMITIVE_TOPOLOGY* const)_malloca(material_count * sizeof(D3D_PRIMITIVE_TOPOLOGY)),
            //    (UINT* const)_malloca(material_count * sizeof(UINT))
            //};

            //content::sub_mesh::get_views(material_count,  );

            std::unique_ptr<UINT[]>items{ std::make_unique<UINT[]>(sizeof(UINT) * (1 + (UINT64)material_count + 1)) };

            items[0] = geometry_content_id;
            UINT* const item_ids{ &items[1] };

            d3d12_render_item* const d3d12_items{ (d3d12_render_item* const)_malloca(material_count * sizeof(d3d12_render_item)) };

            for (UINT i{ 0 }; i < material_count; ++i)
            {
                d3d12_render_item& item{ d3d12_items[i] };

                //const sub_mesh_view& view{ sub_mesh_views[gpu_ids[i]] };

                item.entity_id = entity_id;
                item.sub_mesh_gpu_id = gpu_ids[i];
                item.material_id = material_ids[i];

                D3D_PRIMITIVE_TOPOLOGY primitive_topology{ sub_mesh::get_primitive_topology(gpu_ids[i]) };
                UINT element_type{ sub_mesh::get_views_element_type(gpu_ids[i]) };
                item.graphic_pass_pso_id = create_graphic_pso(item.material_id, primitive_topology, element_type);
                item.depth_pso_id = create_depth_pso(item.material_id, primitive_topology, element_type);
            }

            std::lock_guard lock{ render_item_mutex };
            for (UINT i{ 0 }; i < material_count; ++i)
            {
                item_ids[i] = render_items.add(d3d12_items[i]);
            }

            // mark the end of ids list.
            item_ids[material_count] = Invalid_Index;

            _freea(d3d12_items);
            _freea(gpu_ids);
            UINT item_id = render_item_ids.add(std::move(items));
            return item_id;
        }

        void remove(UINT id)
        {
            std::lock_guard lock{ render_item_mutex };
            const UINT* const item_ids{ &render_item_ids[id][1] };

            // NOTE: the last element in the list of ids is always an invalid id.
            for (UINT i{ 0 }; item_ids[i] != Invalid_Index; ++i)
            {
                render_items.remove(item_ids[i]);
            }

            render_item_ids.remove(id);
        }

        void get_d3d12_render_item_ids(const core::frame_info& info, utl::vector<UINT>& d3d12_render_item_ids)
        {
            assert(info.render_item_ids && info.render_item_count);
            assert(d3d12_render_item_ids.empty());

            frame_cache.lod_offsets_counts.clear();
            frame_cache.geometry_ids.clear();
            const UINT count{ info.render_item_count };

            std::lock_guard lock{ render_item_mutex };

            for (UINT i{ 0 }; i < count; ++i)
            {
                const UINT* const buffer{ render_item_ids[info.render_item_ids[i]].get() };
                frame_cache.geometry_ids.emplace_back(buffer[0]);
            }

            get_lod_offsets_counts(frame_cache.geometry_ids.data(), info.thresholds, count, frame_cache.lod_offsets_counts);
            assert(frame_cache.lod_offsets_counts.size() == count);

            UINT d3d12_render_item_count{ 0 };
            for (UINT i{ 0 }; i < count; ++i)
            {
                d3d12_render_item_count += frame_cache.lod_offsets_counts[i].count;
            }

            assert(d3d12_render_item_count);
            d3d12_render_item_ids.resize(d3d12_render_item_count);

            UINT item_index{ 0 };
            for (UINT i{ 0 }; i < count; ++i)
            {
                const UINT* const item_ids{ &render_item_ids[info.render_item_ids[i]][1] };
                const level_of_detail_offset_count& lod_offset_count{ frame_cache.lod_offsets_counts[i] };
                memcpy(&d3d12_render_item_ids[item_index], &item_ids[lod_offset_count.offset], sizeof(UINT) * lod_offset_count.count);
                item_index += lod_offset_count.count;
                assert(item_index <= d3d12_render_item_count);
            }

            assert(item_index == d3d12_render_item_count);
        }

        void get_items(const UINT* const d3d12_render_item_ids, UINT id_count, const graphic_pass::graphic_cache& cache)
        {
            assert(d3d12_render_item_ids && id_count);
            assert(cache.entity_ids && cache.sub_mesh_gpu_ids && cache.material_ids &&
                cache.graphic_pipeline_states && cache.depth_pipeline_states);

            std::lock_guard lock1{ render_item_mutex };
            std::lock_guard lock2{ pso_mutex };

            for (UINT i{ 0 }; i < id_count; ++i)
            {
                UINT id = d3d12_render_item_ids[i];
                const d3d12_render_item& item{ render_items[id] };
                cache.entity_ids[i] = item.entity_id;
                cache.sub_mesh_gpu_ids[i] = item.sub_mesh_gpu_id;
                cache.material_ids[i] = item.material_id;
                cache.graphic_pipeline_states[i] = pipeline_states[item.graphic_pass_pso_id];
                cache.depth_pipeline_states[i] = pipeline_states[item.depth_pso_id];
            }
        }
    } // namespace render_item


    bool initialize()
    {
        return true;
    }

    void shutdown()
    {
        for (auto& item : root_signatures)
        {
            core::release(item);
        }

        material_root_signature_map.clear();

        root_signatures.clear();

        for (auto& item : pipeline_states)
        {
            core::release(item);
        }

        pso_map.clear();
        pipeline_states.clear();
    }

    // ContentToEngine.cpp
    UINT create_resource(const void* const data, asset_type::type type)
    {
        assert(data);
        UINT id = Invalid_Index;

        switch (type)
        {
        case asset_type::material: id = create_material_resource(data); break;
        case asset_type::mesh: id = create_geometry_resource(data); break;
        case asset_type::texture: id = create_texture_resource(data); break;
        }

        assert(id != Invalid_Index);
        return id;
    }

    void destroy_resource(const UINT id, asset_type::type type)
    {
        assert(id != Invalid_Index);
        switch (type)
        {
        case asset_type::material: destroy_material_resource(id); break;
        case asset_type::mesh: destroy_geometry_resource(id); break;
        case asset_type::texture: destroy_texture_resource(id); break;
        }
    }

    void get_sub_mesh_gpu_ids(UINT geometry_context_id, UINT id_count, UINT* const gpu_ids)
    {
        std::lock_guard lock{ geometry_mutex };
        UINT8* const pointer{ geometry_hierarchies[geometry_context_id] };

        if ((uintptr_t)pointer & single_mesh_marker)
        {
            assert(id_count);
            *gpu_ids = gpu_id_from_fake_pointer(pointer);
        }
        else
        {
            struct geometry_data* ptr = (geometry_data*)pointer;
            const UINT level_of_detail_count{ ptr->level_of_detail_count };
            assert(level_of_detail_count);
            level_of_detail_offset_count* lod_offset_count{ (level_of_detail_offset_count*)&pointer[sizeof(UINT) + (sizeof(float) * level_of_detail_count)] };
            UINT* const geometry_gpu_ids{ (UINT*)&pointer[sizeof(UINT) + ((sizeof(float) + sizeof(level_of_detail_offset_count)) * level_of_detail_count)] };

            assert([&]() {
                const UINT gpu_id_count{ (UINT)lod_offset_count->offset + (UINT)lod_offset_count->count };
                return gpu_id_count == id_count;
                }());

            memcpy(gpu_ids, geometry_gpu_ids, sizeof(UINT) * id_count);
        }

    }

    void get_lod_offsets_counts(const UINT* const geometry_ids, const float* thresholds, UINT id_count, utl::vector<level_of_detail_offset_count>& offsets_counts)
    {
        assert(geometry_ids && id_count);
        assert(offsets_counts.empty());

        std::lock_guard lock{ geometry_mutex };

        for (UINT i{ 0 }; i < id_count; ++i)
        {
            UINT8* const pointer{ geometry_hierarchies[geometry_ids[i]] };
            if ((uintptr_t)pointer & single_mesh_marker)
            {
                offsets_counts.emplace_back(level_of_detail_offset_count{ 0, 1 });
            }
            else
            {
                struct geometry_data* ptr = (geometry_data*)pointer;
                const UINT level_of_detail_count{ ptr->level_of_detail_count };
                float* const thresholds{ (float*)&pointer[sizeof(UINT)] };
                level_of_detail_offset_count* lod_offset_count{ (level_of_detail_offset_count*)&pointer[sizeof(UINT) + (sizeof(float) * level_of_detail_count)] };
                UINT lod{ lod_from_threshold(thresholds[i], thresholds, level_of_detail_count) };
                offsets_counts.emplace_back(lod_offset_count[lod]);
            }
        }
    }
}
