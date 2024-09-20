#pragma once
#include "stdafx.h"
#include "Vector.h"
#include "Content.h"

namespace d3d12 {
    struct d3d12_frame_info;
}

namespace graphic_pass {
    struct opaque_root_parameter {
        enum parameter : UINT {
            global_shader_data,

            count
        };
    };

    struct graphic_cache
    {
        utl::vector<UINT> d3d12_render_item_ids;
        UINT descriptor_index_count{ 0 };

        // allocated items
        UINT* entity_ids{ nullptr };
        UINT* sub_mesh_gpu_ids{ nullptr };
        UINT* material_ids{ nullptr };
        ID3D12PipelineState** graphic_pass_pipeline_states{ nullptr };
        ID3D12PipelineState** depth_pipeline_states{ nullptr };
        ID3D12RootSignature** root_signatures{ nullptr };
        content::material_type::type* material_types{ nullptr };
        UINT** descriptor_indices{ nullptr };
        UINT* texture_counts{ nullptr };
        //D3D12_GPU_VIRTUAL_ADDRESS* position_buffers{ nullptr };
        D3D12_VERTEX_BUFFER_VIEW* position_buffer_views{ nullptr };
        D3D12_GPU_VIRTUAL_ADDRESS* element_buffers{ nullptr };
        D3D12_INDEX_BUFFER_VIEW* index_buffer_views{ nullptr };
        D3D_PRIMITIVE_TOPOLOGY* primitive_topologies{ nullptr };
        UINT* elements_types{ nullptr };
        D3D12_GPU_VIRTUAL_ADDRESS* per_object_data{ nullptr };
        D3D12_GPU_VIRTUAL_ADDRESS* srv_indices{ nullptr };

        constexpr UINT size() const
        {
            return (UINT)d3d12_render_item_ids.size();
        }

        constexpr void clear()
        {
            d3d12_render_item_ids.clear();
            descriptor_index_count = 0;
        }

        constexpr void resize()
        {
            const UINT64 items_count{ d3d12_render_item_ids.size() };
            const UINT64 new_buffer_size{ items_count * struct_size };
            const UINT64 old_buffer_size{ m_buffer.size() };
            if (new_buffer_size > old_buffer_size)
            {
                m_buffer.resize(new_buffer_size);
            }

            if (new_buffer_size != old_buffer_size)
            {
                entity_ids = (UINT*)m_buffer.data();
                sub_mesh_gpu_ids = (UINT*)&entity_ids[items_count];
                material_ids = (UINT*)&sub_mesh_gpu_ids[items_count];
                graphic_pass_pipeline_states = (ID3D12PipelineState**)&material_ids[items_count];
                depth_pipeline_states = (ID3D12PipelineState**)&graphic_pass_pipeline_states[items_count];
                root_signatures = (ID3D12RootSignature**)&depth_pipeline_states[items_count];
                material_types = (content::material_type::type*)&root_signatures[items_count];
                descriptor_indices = (UINT**)&material_types[items_count];
                texture_counts = (UINT*)&descriptor_indices[items_count];
                //position_buffers = (D3D12_GPU_VIRTUAL_ADDRESS*)&texture_counts[items_count];
                position_buffer_views = (D3D12_VERTEX_BUFFER_VIEW*)&texture_counts[items_count];
                element_buffers = (D3D12_GPU_VIRTUAL_ADDRESS*)&position_buffer_views[items_count];
                index_buffer_views = (D3D12_INDEX_BUFFER_VIEW*)&element_buffers[items_count];
                primitive_topologies = (D3D_PRIMITIVE_TOPOLOGY*)&index_buffer_views[items_count];
                elements_types = (UINT*)&primitive_topologies[items_count];
                per_object_data = (D3D12_GPU_VIRTUAL_ADDRESS*)&elements_types[items_count];
                srv_indices = (D3D12_GPU_VIRTUAL_ADDRESS*)&per_object_data[items_count];
            }
        }

    private:
        constexpr static UINT struct_size{
            sizeof(UINT) +                                   // entity_ids
            sizeof(UINT) +                                   // sub_mesh_gpu_ids
            sizeof(UINT) +                                   // material_ids
            sizeof(ID3D12PipelineState*) +                   // graphic_pass_pipeline_states
            sizeof(ID3D12PipelineState*) +                   // depth_pipeline_states
            sizeof(ID3D12RootSignature*) +                   // root_signatures
            sizeof(content::material_type::type) +           // material_types
            sizeof(UINT*) +                                  // descriptor_indices
            sizeof(UINT) +                                   // texture_counts
            //sizeof(D3D12_GPU_VIRTUAL_ADDRESS) +              // position_buffers
            sizeof(D3D12_VERTEX_BUFFER_VIEW) +              // position_buffers
            sizeof(D3D12_GPU_VIRTUAL_ADDRESS) +              // element_buffers
            sizeof(D3D12_INDEX_BUFFER_VIEW) +                // index_buffer_views
            sizeof(D3D_PRIMITIVE_TOPOLOGY) +                 // primitive_topologies
            sizeof(UINT) +                                   // elements_types
            sizeof(D3D12_GPU_VIRTUAL_ADDRESS) +              // per_object_data
            sizeof(D3D12_GPU_VIRTUAL_ADDRESS)                // srv_indices
        };

        utl::vector<UINT8> m_buffer;
    };


    bool initialize();
    void shutdown();

    void set_size(DirectX::XMUINT2 size);
    void depth_prepass(id3d12_graphics_command_list* cmd_list, const core::d3d12_frame_info& d3d12_info);
    void render(id3d12_graphics_command_list* cmd_list, const core::d3d12_frame_info& d3d12_info);
}
