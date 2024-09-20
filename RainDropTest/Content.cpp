#include "Content.h"
#include "Math.h"
#include "Buffers.h"
#include "FreeList.h"
#include "Main.h"
#include "Helpers.h"
#include "Math.h"
#include <unordered_map>
#include "GraphicPass.h"
#include "Core.h"

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

        utl::free_list<ID3D12Resource*> sub_mesh_buffers{ 1 };
        utl::free_list<sub_mesh_view> sub_mesh_views{ 2 };
        std::mutex sub_mesh_mutex{};

        // material
        utl::vector<ID3D12RootSignature*> root_signatures;
        std::unordered_map<UINT64, UINT> material_root_signature_map;
        utl::free_list<std::unique_ptr<UINT8[]>> materials{ 3 };
        std::mutex material_mutex{};

        utl::free_list<d3d12_render_item> render_items;
        utl::free_list<std::unique_ptr<UINT[]>>render_item_ids;
        std::mutex render_item_mutex{};

        utl::vector<ID3D12PipelineState*> pipeline_states;
        std::unordered_map<UINT64, UINT> pso_map;
        std::mutex pso_mutex{};

        struct {
            utl::vector<level_of_detail_offset_count> lod_offsets_counts;
            utl::vector<UINT> geometry_ids;
        } frame_cache;

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
                using params = opaque_root_parameter;
                d3dx::d3d12_root_parameter parameters[params::count]{};
                parameters[params::global_shader_data].as_cbv(D3D12_SHADER_VISIBILITY_ALL, 0);

                root_signature = d3dx::d3d12_root_signature_desc
                {
                    //nullptr, 0, get_root_signature_flags(flags) | D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
                    &parameters[0], _countof(parameters), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
                    nullptr, 0
                }.create();
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

        UINT create_main_pso(UINT material_id, D3D12_PRIMITIVE_TOPOLOGY primitive_topology, UINT elements_type)
        {
            constexpr UINT64 aligned_stream_size{ math::align_size_up<sizeof(UINT64)>(sizeof(d3dx::d3d12_pipeline_state_subobject_stream)) };
            UINT8* const stream_ptr{ (UINT8* const)_malloca(aligned_stream_size) };
            ZeroMemory(stream_ptr, aligned_stream_size);
            new (stream_ptr) d3dx::d3d12_pipeline_state_subobject_stream{};

            d3dx::d3d12_pipeline_state_subobject_stream& stream{ *(d3dx::d3d12_pipeline_state_subobject_stream* const)stream_ptr };

            // lock materials
            {
                std::lock_guard lock{ material_mutex };

                // Define the vertex input layout.
                D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
                {
                    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                };

                const UINT8* const material_ptr = (UINT8* const)materials[material_id].get();
                content::material::material_header* material_header_ptr = (content::material::material_header*)material_ptr;

                D3D12_RT_FORMAT_ARRAY rt_array{};
                rt_array.NumRenderTargets = 1;
                rt_array.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

                stream.render_target_formats = rt_array;
                stream.input_layout = { inputElementDescs, _countof(inputElementDescs) };
                stream.root_signature = root_signatures[material_header_ptr->root_signature_id];
                stream.primitive_topology = get_d3d_primitive_topology_type(primitive_topology);
                stream.depth_stencil_format = DXGI_FORMAT_D32_FLOAT;

                stream.rasterizer = d3dx::rasterizer_state.face_cull;
                stream.depth_stencil1 = d3dx::depth_state.reversed;
                stream.blend = d3dx::blend_state.disabled;

                const shaders::shader_flags::flags flags{ material_header_ptr->flags };

                const UINT key{ shaders::element_type_to_shader_id(elements_type) };
                stream.vs = shaders::get_engine_shader(key);
                stream.ps = shaders::get_engine_shader(shaders::engine_shader::pixel_shader_ps);
                stream.gs = D3D12_SHADER_BYTECODE{};
            }

            UINT pso_id = create_pso_if_needed(stream_ptr, aligned_stream_size);
            free(stream_ptr);
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

                // Define the vertex input layout.
                D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
                {
                    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                };

                const UINT8* const material_ptr = (UINT8* const)materials[material_id].get();
                content::material::material_header* material_header_ptr = (content::material::material_header*)material_ptr;

                D3D12_RT_FORMAT_ARRAY rt_array{};
                rt_array.NumRenderTargets = 1;
                rt_array.RTFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;

                stream.render_target_formats = rt_array;
                stream.input_layout = { inputElementDescs, _countof(inputElementDescs) };
                stream.root_signature = root_signatures[material_header_ptr->root_signature_id];
                stream.primitive_topology = get_d3d_primitive_topology_type(primitive_topology);
                stream.depth_stencil_format = DXGI_FORMAT_D32_FLOAT;
                stream.rasterizer = d3dx::rasterizer_state.face_cull;


                D3D12_DEPTH_STENCIL_DESC1 depth{};
                depth.DepthEnable = TRUE;
                depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
                depth.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
                depth.StencilEnable = FALSE;
                depth.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
                depth.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
                const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
                depth.FrontFace = defaultStencilOp;
                depth.BackFace = defaultStencilOp;

                stream.depth_stencil1 = depth;
                //stream.depth_stencil1 = d3dx::depth_state.reversed_readonly;


                D3D12_BLEND_DESC blend{};
                blend.AlphaToCoverageEnable = FALSE;
                blend.IndependentBlendEnable = FALSE;
                D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc{};
                defaultRenderTargetBlendDesc.BlendEnable = FALSE;
                defaultRenderTargetBlendDesc.LogicOpEnable = FALSE;
                defaultRenderTargetBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
                defaultRenderTargetBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
                defaultRenderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
                defaultRenderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
                defaultRenderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
                defaultRenderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
                defaultRenderTargetBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
                defaultRenderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
                for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
                    blend.RenderTarget[i] = defaultRenderTargetBlendDesc;

                stream.blend = blend;

                const shaders::shader_flags::flags flags{ material_header_ptr->flags };

                const UINT key{ shaders::element_type_to_shader_id(elements_type) };
                stream.vs = shaders::get_engine_shader(key);
                stream.ps = D3D12_SHADER_BYTECODE{};
            }

            UINT pso_id = create_pso_if_needed(stream_ptr, aligned_stream_size);

            free(stream_ptr);
            return pso_id;
        }

    } // anonymous namespace

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

        void get_views(UINT id_count, const graphic_pass::graphic_cache& cache)
        {
            assert(cache.sub_mesh_gpu_ids && id_count);
            assert(cache.position_buffer_views && cache.element_buffers && cache.index_buffer_views &&
                cache.primitive_topologies && cache.elements_types);

            std::lock_guard lock{ sub_mesh_mutex };
            for (UINT i{ 0 }; i < id_count; ++i)
            {
                const sub_mesh_view& view{ sub_mesh_views[cache.sub_mesh_gpu_ids[i]] };
                cache.position_buffer_views[i] = view.position_buffer_view;
                cache.element_buffers[i] = view.element_buffer_view.BufferLocation;
                cache.index_buffer_views[i] = view.index_buffer_view;
                cache.primitive_topologies[i] = view.primitive_topology;
                cache.elements_types[i] = view.element_type;
            }
        }
    } // namespace sub_mesh

    namespace material {
        // Output format:
        //
        // struct {
        // material_type::type  type,
        // shader_flags::flags  flags,
        // id::id_type          root_signature_id,
        // u32                  texture_count,
        // u32                  shader_count,
        // id::id_type          shader_ids[shader_count],
        // id::id_type          texture_ids[texture_count],
        // u32*                 descriptor_indices[texture_count]
        // } d3d12_material
        UINT add(content::material_init_info info)
        {
            std::unique_ptr<UINT8[]> buffer;
            std::lock_guard lock{ material_mutex };

            UINT shader_count{ 0 };
            UINT shader_flags{ 0 };
            for (UINT i{ 0 }; i < shaders::shader_type::count; ++i)
            {
                if (info.shader_ids[i] != Invalid_Index)
                {
                    ++shader_count;
                    shader_flags |= (1 << i);
                }
            }

            UINT64 material_output_size{
                (sizeof(content::material::material_header) +
                 sizeof(UINT) * shader_count +
                 sizeof(UINT) * info.texture_count +
                 sizeof(UINT) * info.texture_count) };
            buffer = std::make_unique<UINT8[]>(material_output_size);
            content::material::material_header* header = (content::material::material_header*)buffer.get();
            UINT* const shader_ids = (UINT*)&buffer[(UINT)(sizeof(content::material::material_header))];
            UINT* const texture_ids = (UINT*)&buffer[(UINT)(sizeof(content::material::material_header))];

            header->type = info.type;
            header->flags = *(shaders::shader_flags::flags*)&shader_flags;
            header->shader_count = shader_count;
            header->texture_count = info.texture_count;

            header->root_signature_id = create_root_signature(header->type, header->flags);
            // TODO: textures
            //if (info.texture_count)
            //{

            //}

            UINT shader_index{ 0 };
            for (UINT i{ 0 }; i < shaders::shader_type::count; ++i)
            {
                if (info.shader_ids[i] != Invalid_Index)
                {
                    shader_ids[shader_index] = info.shader_ids[i];
                    ++shader_index;
                }
            }

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

        void get_materials(UINT id_count, const graphic_pass::graphic_cache& cache)
        {
            assert(cache.material_ids && id_count);
            assert(cache.root_signatures && cache.material_types);

            std::lock_guard lock{ material_mutex };

            for (UINT i{ 0 }; i < id_count; ++i)
            {
                const UINT8* buffer{ materials[cache.material_ids[i]].get() };
                content::material::material_header* header = (content::material::material_header*)buffer;
                UINT* const shader_ids = (UINT*)&buffer[(UINT)(sizeof(content::material::material_header))];
                UINT* const texture_ids = (UINT*)&buffer[(UINT)(sizeof(content::material::material_header))];

                cache.root_signatures[i] = root_signatures[header->root_signature_id];
                cache.material_types[i] = header->type;
                //cache.texture_count[i] = stream.texture_count();
                //total_index_count += stream.texture_count();
            }

            //descriptor_index_count = total_index_count;
        }

    } // namespace material

    namespace render_item {

        UINT add(UINT entity_id, UINT geometry_content_id, UINT material_count, const UINT* const material_ids)
        {
            assert(entity_id != Invalid_Index && geometry_content_id != Invalid_Index);
            assert(material_count && material_ids);

            UINT* const gpu_ids{ (UINT* const)_malloca(material_count * sizeof(UINT)) };
            content::get_sub_mesh_gpu_ids(geometry_content_id, material_count, gpu_ids);

            //sub_mesh::views_cache views_cache
            //{
            //    (D3D12_GPU_VIRTUAL_ADDRESS* const)_malloca(material_count * sizeof(D3D12_GPU_VIRTUAL_ADDRESS)),
            //    (D3D12_GPU_VIRTUAL_ADDRESS* const)_malloca(material_count * sizeof(D3D12_GPU_VIRTUAL_ADDRESS)),
            //    (D3D12_INDEX_BUFFER_VIEW* const)_malloca(material_count * sizeof(D3D12_INDEX_BUFFER_VIEW)),
            //    (D3D_PRIMITIVE_TOPOLOGY* const)_malloca(material_count * sizeof(D3D_PRIMITIVE_TOPOLOGY)),
            //    (UINT* const)_malloca(material_count * sizeof(UINT))
            //};

            //sub_mesh::get_views(gpu_ids, material_count, views_cache);

            std::unique_ptr<UINT[]>items{ std::make_unique<UINT[]>(sizeof(UINT) * (1 + (UINT64)material_count + 1)) };

            items[0] = geometry_content_id;
            UINT* const item_ids{ &items[1] };

            d3d12_render_item* const d3d12_items{ (d3d12_render_item* const)_malloca(material_count * sizeof(d3d12_render_item)) };

            for (UINT i{ 0 }; i < material_count; ++i)
            {
                d3d12_render_item& item{ d3d12_items[i] };

                const sub_mesh_view& view{ sub_mesh_views[gpu_ids[i]] };

                item.entity_id = entity_id;
                item.sub_mesh_gpu_id = gpu_ids[i];
                item.material_id = material_ids[i];
                //item.graphic_pass_pso_id = create_main_pso(item.material_id, views_cache.primitive_topologies[i], views_cache.elements_types[i]);
                item.graphic_pass_pso_id = create_main_pso(item.material_id, view.primitive_topology, view.element_type);
                item.depth_pso_id = create_depth_pso(item.material_id, view.primitive_topology, view.element_type);
            }

            std::lock_guard lock{ render_item_mutex };
            for (UINT i{ 0 }; i < material_count; ++i)
            {
                item_ids[i] = render_items.add(d3d12_items[i]);
            }

            // mark the end of ids list.
            item_ids[material_count] = Invalid_Index;
            free(d3d12_items);
            free(gpu_ids);
            return render_item_ids.add(std::move(items));
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

        //void get_items(const UINT* const d3d12_render_item_ids, UINT id_count, const items_cache& cache)
        //{
        //    assert(d3d12_render_item_ids && id_count);
        //    assert(cache.entity_ids && cache.sub_mesh_gpu_ids && cache.material_ids &&
        //        cache.graphic_pass_psos && cache.depth_psos);

        //    std::lock_guard lock1{ render_item_mutex };
        //    std::lock_guard lock2{ pso_mutex };

        //    for (UINT i{ 0 }; i < id_count; ++i)
        //    {
        //        const d3d12_render_item& item{ render_items[d3d12_render_item_ids[i]] };
        //        cache.entity_ids[i] = item.entity_id;
        //        cache.sub_mesh_gpu_ids[i] = item.sub_mesh_gpu_id;
        //        cache.material_ids[i] = item.material_id;
        //        cache.graphic_pass_psos[i] = pipeline_states[item.graphic_pass_pso_id];
        //        cache.depth_psos[i] = pipeline_states[item.depth_pso_id];
        //    }
        //}

        void get_items(const UINT* const d3d12_render_item_ids, UINT id_count, const graphic_pass::graphic_cache& cache)
        {
            assert(d3d12_render_item_ids && id_count);
            assert(cache.entity_ids && cache.sub_mesh_gpu_ids && cache.material_ids &&
                cache.graphic_pass_pipeline_states && cache.depth_pipeline_states);

            std::lock_guard lock1{ render_item_mutex };
            std::lock_guard lock2{ pso_mutex };

            for (UINT i{ 0 }; i < id_count; ++i)
            {
                UINT id = d3d12_render_item_ids[i];
                const d3d12_render_item& item{ render_items[id] };
                cache.entity_ids[i] = item.entity_id;
                cache.sub_mesh_gpu_ids[i] = item.sub_mesh_gpu_id;
                cache.material_ids[i] = item.material_id;
                cache.graphic_pass_pipeline_states[i] = pipeline_states[item.graphic_pass_pso_id];
                cache.depth_pipeline_states[i] = pipeline_states[item.depth_pso_id];
            }
        }
    } // namespace render_item

}
