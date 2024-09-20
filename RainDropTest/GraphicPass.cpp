#include "GraphicPass.h"
#include "Vector.h"
#include "Content.h"
#include "RainDrop.h"
#include "SharedTypes.h"
#include "Transform.h"

namespace graphic_pass
{
    namespace {

        graphic_cache frame_cache;

        void set_root_parameters(id3d12_graphics_command_list* const cmd_list, UINT cache_index)
        {
            const graphic_cache& cache{ frame_cache };
            assert(cache_index < cache.size());

            const content::material_type::type mtl_type{ cache.material_types[cache_index] };
            switch (mtl_type)
            {
            case content::material_type::opaque:
            {
                //using params = opaque_root_parameter;
                //cmd_list->SetGraphicsRootShaderResourceView(params::position_buffer, cache.position_buffers[cache_index]);
                //cmd_list->SetGraphicsRootShaderResourceView(params::element_buffer, cache.element_buffers[cache_index]);
                //cmd_list->SetGraphicsRootConstantBufferView(params::per_object_data, cache.per_object_data[cache_index]);
                //if (cache.texture_counts[cache_index])
                //{
                //    cmd_list->SetGraphicsRootShaderResourceView(params::srv_indices, cache.srv_indices[cache_index]);
                //}
            }
            break;
            }
        }

        void fill_per_object_data(const core::d3d12_frame_info& d3d12_info)
        {
            const graphic_cache& cache{ frame_cache };
            const UINT render_items_count{ (UINT)cache.size() };
            UINT current_entity_id{ Invalid_Index };
            
            hlsl::PerObjectData* current_data_pointer{ nullptr };

            for (UINT i{ 0 }; i < render_items_count; ++i)
            {
                current_entity_id = cache.entity_ids[i];
                hlsl::PerObjectData data{};
                transform::get_transform_matrices(current_entity_id, data.World, data.InvWorld);
                XMMATRIX world{ XMLoadFloat4x4(&data.World) };
            }
             
        }

        void prepare_render_frame(const core::d3d12_frame_info& d3d12_info)
        {
            assert(d3d12_info.info);
            graphic_cache& cache{ frame_cache };
            cache.clear();

            content::render_item::get_d3d12_render_item_ids(*d3d12_info.info, cache.d3d12_render_item_ids);
            cache.resize();

            const UINT item_count{ cache.size() };
            content::render_item::get_items(cache.d3d12_render_item_ids.data(), item_count, cache);

            content::sub_mesh::get_views(item_count, cache);

            content::material::get_materials(item_count, cache);

            fill_per_object_data(d3d12_info);
        }
    }

    bool initialize()
    {
        return true;
    }

    void shutdown()
    {}

    void set_size(DirectX::XMUINT2 size)
    {}

    void render(id3d12_graphics_command_list* cmd_list, const core::d3d12_frame_info& d3d12_info)
    {
        prepare_render_frame(d3d12_info);

        const graphic_cache& cache{ frame_cache };
        const UINT items_count{ cache.size() };

        ID3D12RootSignature* current_root_signature{ nullptr };
        ID3D12PipelineState* current_pipeline_state{ nullptr };

        for (UINT i{ 0 }; i < items_count; ++i)
        {
            if (current_root_signature != cache.root_signatures[i])
            {
                using idx = opaque_root_parameter;

                current_root_signature = cache.root_signatures[i];
                cmd_list->SetGraphicsRootSignature(current_root_signature);
                cmd_list->SetGraphicsRootConstantBufferView(idx::global_shader_data, d3d12_info.global_shader_data);
                //cmd_list->SetGraphicsRootShaderResourceView(idx::directional_lights, light::non_cullable_light_buffer(frame_index));
                //cmd_list->SetGraphicsRootShaderResourceView(idx::cullable_lights, light::cullable_light_buffer(frame_index));
                //cmd_list->SetGraphicsRootShaderResourceView(idx::light_grid, delight::light_grid_opaque(light_culling_id, frame_index));
                //cmd_list->SetGraphicsRootShaderResourceView(idx::light_index_list, delight::light_index_list_opaque(light_culling_id, frame_index));
            }

            if (current_pipeline_state != cache.graphic_pass_pipeline_states[i])
            {
                current_pipeline_state = cache.graphic_pass_pipeline_states[i];
                cmd_list->SetPipelineState(current_pipeline_state);
            }

            //set_root_parameters(cmd_list, i);

            const D3D12_INDEX_BUFFER_VIEW& ibv{ cache.index_buffer_views[i] };
            const UINT index_count{ ibv.SizeInBytes >> (ibv.Format == DXGI_FORMAT_R16_UINT ? 1 : 2) };

            cmd_list->IASetIndexBuffer(&ibv);

            const D3D12_VERTEX_BUFFER_VIEW vbv{ cache.position_buffer_views[i] };
            cmd_list->IASetVertexBuffers(0, 1, &vbv);
            
            cmd_list->IASetPrimitiveTopology(cache.primitive_topologies[i]);
            
            cmd_list->DrawIndexedInstanced(index_count, 1, 0, 0, 0);
        }

    }

    //void depth_prepass(id3d12_graphics_command_list* cmd_list, const d3d12_frame_info& d3d12_info)
    //{
    //    prepare_render_frame(d3d12_info);

    //    const graphic_cache& cache{ frame_cache };
    //    const UINT items_count{ cache.size() };

    //    ID3D12RootSignature* current_root_signature{ nullptr };
    //    ID3D12PipelineState* current_pipeline_state{ nullptr };

    //    for (UINT i{ 0 }; i < items_count; ++i)
    //    {
    //        if (current_root_signature != cache.root_signatures[i])
    //        {
    //            current_root_signature = cache.root_signatures[i];
    //            cmd_list->SetGraphicsRootSignature(current_root_signature);
    //            //cmd_list->SetGraphicsRootConstantBufferView(opaque_root_parameter::global_shader_data, d3d12_info.global_shader_data);
    //        }

    //        if (current_pipeline_state != cache.depth_pipeline_states[i])
    //        {
    //            current_pipeline_state = cache.depth_pipeline_states[i];
    //            cmd_list->SetPipelineState(current_pipeline_state);
    //        }

    //        set_root_parameters(cmd_list, i);

    //        const D3D12_INDEX_BUFFER_VIEW& ibv{ cache.index_buffer_views[i] };
    //        const UINT index_count{ ibv.SizeInBytes >> (ibv.Format == DXGI_FORMAT_R16_UINT ? 1 : 2) };

    //        cmd_list->IASetIndexBuffer(&ibv);
    //        cmd_list->IASetPrimitiveTopology(cache.primitive_topologies[i]);
    //        cmd_list->DrawIndexedInstanced(index_count, 1, 0, 0, 0);
    //    }
    //}
}
