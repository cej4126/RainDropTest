#pragma once
#include "stdafx.h"
#include "Vector.h"
#include "Content.h"
#include "Barriers.h"
#include "Resources.h"

namespace d3d12 {
    struct d3d12_frame_info;
}

namespace graphic_pass {

    constexpr DXGI_FORMAT main_buffer_format{ DXGI_FORMAT_R16G16B16A16_FLOAT };
    constexpr DXGI_FORMAT depth_buffer_format{ DXGI_FORMAT_D32_FLOAT };

    struct opaque_root_parameter {
        enum parameter : UINT {
            global_shader_data,
            per_object_data,
            position_buffer,
            element_buffer,
            srv_indices,
            directional_lights,
            cullable_lights,
            light_grid,
            light_index_list,

            count
        };
    };

    struct graphic_cache
    {
        utl::vector<UINT> d3d12_render_item_ids;
        UINT descriptor_index_count{ 0 };

        // allocated items
        // TODO: add const
        // items_cache
        UINT* entity_ids{ nullptr };
        UINT* sub_mesh_gpu_ids{ nullptr };
        UINT* material_ids{ nullptr };
        ID3D12PipelineState** graphic_pipeline_states{ nullptr };
        ID3D12PipelineState** depth_pipeline_states{ nullptr };
        // material_cache
        ID3D12RootSignature** root_signatures{ nullptr };
        content::material_type::type* material_types{ nullptr };
        UINT** descriptor_indices{ nullptr };
        UINT* texture_counts{ nullptr };
        content::material_surface** material_surfaces{ material_surfaces };
        // views_cache
        D3D12_GPU_VIRTUAL_ADDRESS* position_buffers{ nullptr };
        D3D12_GPU_VIRTUAL_ADDRESS* element_buffers{ nullptr };
        D3D12_INDEX_BUFFER_VIEW* index_buffer_views{ nullptr };
        D3D_PRIMITIVE_TOPOLOGY* primitive_topologies{ nullptr };
        UINT* elements_types{ nullptr };

        D3D12_GPU_VIRTUAL_ADDRESS* per_object_data{ nullptr };
        D3D12_GPU_VIRTUAL_ADDRESS* srv_indices{ nullptr };

        constexpr UINT size() const;
        constexpr void clear();
        constexpr void resize();

    private:
        constexpr static UINT struct_size{
            sizeof(UINT) +                                   // entity_ids
            sizeof(UINT) +                                   // sub_mesh_gpu_ids
            sizeof(UINT) +                                   // material_ids
            sizeof(ID3D12PipelineState*) +                   // graphic_pipeline_states
            sizeof(ID3D12PipelineState*) +                   // depth_pipeline_states
            sizeof(ID3D12RootSignature*) +                   // root_signatures
            sizeof(content::material_type::type) +           // material_types
            sizeof(UINT*) +                                  // descriptor_indices
            sizeof(UINT) +                                   // texture_counts
            sizeof(content::material_surface*) +             // material_surfaces
            sizeof(D3D12_GPU_VIRTUAL_ADDRESS) +              // position_buffers
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

    const resource::Render_Target& get_graphic_buffer();
    const resource::Depth_Buffer& get_depth_buffer();

    void set_size(DirectX::XMUINT2 size);
    void depth_process(id3d12_graphics_command_list* cmd_list, const core::d3d12_frame_info& d3d12_info, barriers::resource_barrier& barriers);
    void render_targets(id3d12_graphics_command_list* cmd_list, const core::d3d12_frame_info& d3d12_info, barriers::resource_barrier& barriers);

}
