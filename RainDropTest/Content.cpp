#include "Content.h"
#include "ContentToEngine.h"
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

        utl::free_list<ID3D12Resource*> sub_mesh_buffers{};
        utl::free_list<sub_mesh_view> sub_mesh_views{};
        std::mutex sub_mesh_mutex{};

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
        {}

    } // namespace sub_mesh
}
