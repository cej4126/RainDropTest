#include "Content.h"
#include "Math.h"
#include "Buffers.h"
#include "FreeList.h"
#include "Main.h"

namespace d3d12::content
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
            UINT pso_id;
            UINT depth_id;
        };

        utl::free_list<ID3D12Resource*> sub_mesh_buffers{ 1 };
        utl::free_list<sub_mesh_view> sub_mesh_views{ 2 };
        std::mutex sub_mesh_mutex{};

        // material
        utl::vector<ID3D12RootSignature*> root_signatures;
        utl::free_list<std::unique_ptr<UINT8[]>> materials{ 3 };
        std::mutex material_mutex{};

        utl::free_list<d3d12_render_item> render_items;
        utl::free_list<std::unique_ptr<UINT[]>>render_item_ids;
        std::mutex render_item_mutex{};

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

        UINT create_main_pso(UINT material_id, D3D12_PRIMITIVE_TOPOLOGY primitive_topology, UINT elements_type)
        {
   //         constexpr UINT64 aligned_stream_size{ math::align_size_up<sizeof(UINT64)>(sizeof(dx3d)) };
            return 0;
        }

    } // anonymous namespace

    bool initialize()
    {
        return true;
    }

    void shutdown()
    {
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

            ID3D12Resource* resource{ buffers::create_buffer_default_with_upload(data, total_buffer_size) };
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

        void get_views(const UINT* const gpu_ids, UINT id_count, const views_cache& cache)
        {
            assert(gpu_ids && id_count);
            assert(cache.position_buffers && cache.element_buffers && cache.index_buffer_views &&
                cache.primitive_topologies && cache.elements_types);

            std::lock_guard lock{ sub_mesh_mutex };
            for (UINT i{ 0 }; i < id_count; ++i)
            {
                const sub_mesh_view& view{ sub_mesh_views[gpu_ids[i]] };
                cache.position_buffers[i] = view.position_buffer_view.BufferLocation;
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
                (sizeof(d3d12::content::material::material_header) +
                 sizeof(UINT) * shader_count +
                 sizeof(UINT) * info.texture_count +
                 sizeof(UINT) * info.texture_count) };
            buffer = std::make_unique<UINT8[]>(material_output_size);
            d3d12::content::material::material_header* header = (d3d12::content::material::material_header*)buffer.get();
            UINT* const shader_ids = (UINT*)&buffer[(UINT)(sizeof(d3d12::content::material::material_header))];
            UINT* const texture_ids = (UINT*)&buffer[(UINT)(sizeof(d3d12::content::material::material_header))];

            header->type = info.type;
            header->flags = *(shaders::shader_flags::flags*)&shader_flags;
            header->shader_count = shader_count;
            header->texture_count = info.texture_count;


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

        void get_materials(const UINT* const material_ids, UINT material_count, const d3d12::content::material::materials_cache& cache, UINT descriptor_index_count)
        {}
    } // namespace material

    namespace render_item {

        UINT add(UINT entity_id, UINT geometry_content_id, UINT material_count, const UINT* const material_ids)
        {
            assert(entity_id != Invalid_Index && geometry_content_id != Invalid_Index);
            assert(material_count && material_ids);

            UINT* const gpu_ids{ (UINT* const)_malloca(material_count * sizeof(UINT)) };
            content::get_sub_mesh_gpu_ids(geometry_content_id, material_count, gpu_ids);

            sub_mesh::views_cache views_cache
            {
                (D3D12_GPU_VIRTUAL_ADDRESS* const)_malloca(material_count * sizeof(D3D12_GPU_VIRTUAL_ADDRESS)),
                (D3D12_GPU_VIRTUAL_ADDRESS* const)_malloca(material_count * sizeof(D3D12_GPU_VIRTUAL_ADDRESS)),
                (D3D12_INDEX_BUFFER_VIEW* const)_malloca(material_count * sizeof(D3D12_INDEX_BUFFER_VIEW)),
                (D3D_PRIMITIVE_TOPOLOGY* const)_malloca(material_count * sizeof(D3D_PRIMITIVE_TOPOLOGY)),
                (UINT* const)_malloca(material_count * sizeof(UINT))
            };

            sub_mesh::get_views(gpu_ids, material_count, views_cache);
            
            std::unique_ptr<UINT[]>items{ std::make_unique<UINT[]>(sizeof(UINT) * (1 + (UINT64)material_count + 1)) };

            items[0] = geometry_content_id;
            UINT* const item_ids{ &items[1] };

            d3d12_render_item* const d3d12_items{ (d3d12_render_item* const)_malloca(material_count * sizeof(d3d12_render_item)) };

            for (UINT i{ 0 }; i < material_count; ++i)
            {
                d3d12_render_item& item{ d3d12_items[i] };
                item.entity_id = entity_id;
                item.sub_mesh_gpu_id = gpu_ids[i];
                item.material_id = material_ids[i];
                item.pso_id = create_main_pso(item.material_id, views_cache.primitive_topologies[i], views_cache.elements_types[i]);
                item.depth_id = Invalid_Index;
            }

            std::lock_guard lock{ render_item_mutex };
            for (UINT i{ 0 }; i < material_count; ++i)
            {
                item_ids[i] = render_items.add(d3d12_items[i]);
            }

            // mark the end of ids list.
            item_ids[material_count] = Invalid_Index;

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

    } // namespace render_item

}
