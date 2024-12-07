#include "GraphicPass.h"
#include "Vector.h"
#include "Content.h"
#include "RainDrop.h"
#include "SharedTypes.h"
#include "Transform.h"
#include "Resources.h"
#include "Lights.h"

namespace graphic_pass
{
    namespace {

        resource::Render_Target graphic_buffer{};
        resource::Depth_Buffer depth_buffer{};
        XMUINT2 initial_dimensions{ 100, 100 };
        XMUINT2 dimensions{ initial_dimensions };
        graphic_cache frame_cache;

#if _DEBUG
        constexpr float clear_value[4]{ 0.5f, 0.5f, 0.5f, 1.f };
#else
        constexpr float clear_value[4]{ };
#endif

        void fill_per_object_data(const core::d3d12_frame_info& d3d12_info)
        {
            const graphic_cache& cache{ frame_cache };
            const UINT render_items_count{ (UINT)cache.size() };
            UINT current_entity_id{ Invalid_Index };
            hlsl::PerObjectData* current_data_pointer{ nullptr };

            resource::constant_buffer& cbuffer{ core::cbuffer() };

            for (UINT i{ 0 }; i < render_items_count; ++i)
            {
                if (current_entity_id != cache.entity_ids[i])
                {
                    current_entity_id = cache.entity_ids[i];
                    hlsl::PerObjectData data{};
                    transform::get_transform_matrices(current_entity_id, data.World, data.InvWorld);
                    XMMATRIX world{ XMLoadFloat4x4(&data.World) };
                    XMMATRIX wvp{ XMMatrixMultiply(world, d3d12_info.camera->view_projection()) };
                    XMStoreFloat4x4(&data.WorldViewProjection, wvp);

                    const content::material_surface* const surface{ cache.material_surfaces[i] };
                    memcpy(&data.BaseColor, surface, sizeof(content::material_surface));

                    current_data_pointer = cbuffer.allocate<hlsl::PerObjectData>();
                    memcpy(current_data_pointer, &data, sizeof(hlsl::PerObjectData));
                }
                assert(current_data_pointer);
                cache.per_object_data[i] = cbuffer.gpu_address(current_data_pointer);
            }
        }

        void prepare_render_frame(const core::d3d12_frame_info& d3d12_info)
        {
            assert(d3d12_info.info && d3d12_info.camera);
            assert(d3d12_info.info->render_item_ids && d3d12_info.info->render_item_count);
            graphic_cache& cache{ frame_cache };
            cache.clear();

            content::render_item::get_d3d12_render_item_ids(*d3d12_info.info, cache.d3d12_render_item_ids);
            cache.resize();

            const UINT items_count{ cache.size() };
            content::render_item::get_items(cache.d3d12_render_item_ids.data(), items_count, cache);

            content::sub_mesh::get_views(items_count, cache);

            content::material::get_materials(items_count, cache);

            fill_per_object_data(d3d12_info);

            if (cache.descriptor_index_count)
            {
                resource::constant_buffer& cbuffer{ core::cbuffer() };
                const UINT size{ cache.descriptor_index_count * sizeof(UINT) };
                UINT* const srv_indices{ (UINT* const)cbuffer.allocate(size) };
                UINT srv_index_offset{ 0 };

                for (UINT i{ 0 }; i < items_count; ++i)
                {
                    const UINT texture_count{ cache.texture_counts[i] };
                    cache.srv_indices[i] = 0;

                    if (texture_count)
                    {
                        const UINT* const descriptor_indices{ cache.descriptor_indices[i] };

                        UINT ind[20];
                        for (UINT j{ 0 }; j < texture_count; ++j)
                        {
                            ind[j] = descriptor_indices[j];
                        }

                        memcpy(&srv_indices[srv_index_offset], descriptor_indices, texture_count * sizeof(UINT));
                        cache.srv_indices[i] = cbuffer.gpu_address(srv_indices + srv_index_offset);
                        srv_index_offset += texture_count;
                    }
                }
            }
        }

        void create_graphic_buffer(XMUINT2 size)
        {
            graphic_buffer.release();

            D3D12_RESOURCE_DESC desc{};

            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;  // D3D12_RESOURCE_DIMENSION Dimension;
            desc.Alignment = 0;                                   // UINT64 Alignment;
            desc.Width = size.x;                                  // UINT64 Width;
            desc.Height = size.y;                                 // UINT Height;
            desc.DepthOrArraySize = 1;                            // UINT16 DepthOrArraySize;
            desc.MipLevels = 0;                                   // UINT16 MipLevels;
            desc.Format = main_buffer_format;                     // DXGI_FORMAT Format;
            desc.SampleDesc = { 1, 0 };                           // DXGI_SAMPLE_DESC SampleDesc;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;           // D3D12_TEXTURE_LAYOUT Layout;
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; // D3D12_RESOURCE_FLAGS Flags;

            resource::texture_init_info info{};
            info.desc = &desc;
            info.initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            info.clear_value.Format = desc.Format;
            memcpy(&info.clear_value.Color, &clear_value[0], sizeof(clear_value));
            graphic_buffer = resource::Render_Target{ info };
            NAME_D3D12_OBJECT(graphic_buffer.resource(), L"Graphic Main Buffer");
        }

        void create_depth_buffer(XMUINT2 size)
        {
            depth_buffer.release();

            D3D12_RESOURCE_DESC desc{};

            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;  // D3D12_RESOURCE_DIMENSION Dimension;
            desc.Alignment = 0;                                   // UINT64 Alignment;
            desc.Width = size.x;                                  // UINT64 Width;
            desc.Height = size.y;                                 // UINT Height;
            desc.DepthOrArraySize = 1;                            // UINT16 DepthOrArraySize;
            desc.MipLevels = 1;                                   // UINT16 MipLevels;
            desc.Format = depth_buffer_format;                     // DXGI_FORMAT Format;
            desc.SampleDesc = { 1, 0 };                           // DXGI_SAMPLE_DESC SampleDesc;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;           // D3D12_TEXTURE_LAYOUT Layout;
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // D3D12_RESOURCE_FLAGS Flags;

            resource::texture_init_info info{};
            info.desc = &desc;
            info.initial_state = D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            info.clear_value.Format = desc.Format;
            info.clear_value.DepthStencil.Depth = 0.f;
            info.clear_value.DepthStencil.Stencil = 0;

            depth_buffer = resource::Depth_Buffer{ info };
            NAME_D3D12_OBJECT(depth_buffer.resource(), L"Graphic Depth Buffer");
        }

        void set_root_parameters(id3d12_graphics_command_list* const cmd_list, UINT cache_index)
        {
            const graphic_cache& cache{ frame_cache };
            assert(cache_index < cache.size());

            const content::material_type::type mtl_type{ cache.material_types[cache_index] };
            switch (mtl_type)
            {
            case content::material_type::opaque:
            {
                using params = content::opaque_root_parameter;
                cmd_list->SetGraphicsRootShaderResourceView(params::position_buffer, cache.position_buffers[cache_index]);
                cmd_list->SetGraphicsRootShaderResourceView(params::element_buffer, cache.element_buffers[cache_index]);
                cmd_list->SetGraphicsRootConstantBufferView(params::per_object_data, cache.per_object_data[cache_index]);
                if (cache.texture_counts[cache_index])
                {
                    cmd_list->SetGraphicsRootShaderResourceView(params::srv_indices, cache.srv_indices[cache_index]);
                }
            }
            break;
            }
        }

    }

    constexpr UINT graphic_cache::size() const
    {
        return (UINT)d3d12_render_item_ids.size();
    }

    constexpr void graphic_cache::clear()
    {
        d3d12_render_item_ids.clear();
        descriptor_index_count = 0;
    }

    constexpr void graphic_cache::resize()
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
            graphic_pipeline_states = (ID3D12PipelineState**)&material_ids[items_count];
            depth_pipeline_states = (ID3D12PipelineState**)&graphic_pipeline_states[items_count];
            root_signatures = (ID3D12RootSignature**)&depth_pipeline_states[items_count];
            material_types = (content::material_type::type*)&root_signatures[items_count];
            descriptor_indices = (UINT**)&material_types[items_count];
            texture_counts = (UINT*)&descriptor_indices[items_count];
            material_surfaces = (content::material_surface**)&texture_counts[items_count];
            position_buffers = (D3D12_GPU_VIRTUAL_ADDRESS*)&material_surfaces[items_count];
            element_buffers = (D3D12_GPU_VIRTUAL_ADDRESS*)&position_buffers[items_count];
            index_buffer_views = (D3D12_INDEX_BUFFER_VIEW*)&element_buffers[items_count];
            primitive_topologies = (D3D_PRIMITIVE_TOPOLOGY*)&index_buffer_views[items_count];
            elements_types = (UINT*)&primitive_topologies[items_count];
            per_object_data = (D3D12_GPU_VIRTUAL_ADDRESS*)&elements_types[items_count];
            srv_indices = (D3D12_GPU_VIRTUAL_ADDRESS*)&per_object_data[items_count];
        }
    }

    bool initialize()
    {
        create_graphic_buffer(initial_dimensions);
        create_depth_buffer(initial_dimensions);
        return true;
    }

    void shutdown()
    {
        graphic_buffer.release();
        depth_buffer.release();
        dimensions = initial_dimensions;
    }

    void set_size(DirectX::XMUINT2 size)
    {
        XMUINT2& d{ dimensions };
        if (size.x > d.x || size.y > d.y)
        {
            d = { (size.x > d.x) ? size.x : d.x, (size.y > d.y) ? size.y : d.y };
            create_graphic_buffer(d);
            create_depth_buffer(d);
        }
    }

    void depth_process(id3d12_graphics_command_list* cmd_list, const core::d3d12_frame_info& d3d12_info, barriers::resource_barrier& barriers)
    {
        // add_transitions_for_depth_pre-pass
        barriers.add(graphic_buffer.resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY);
        barriers.add(depth_buffer.resource(), D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        barriers.apply(cmd_list);

        // set_render_targets_for_depth_pre-pass
        const D3D12_CPU_DESCRIPTOR_HANDLE dsv{ depth_buffer.dsv() };
        cmd_list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.f, 0, 0, nullptr);
        cmd_list->OMSetRenderTargets(0, nullptr, 0, &dsv);

        // depth_pre-pass
        prepare_render_frame(d3d12_info);

        const graphic_cache& cache{ frame_cache };
        const UINT items_count{ cache.size() };

        ID3D12RootSignature* current_root_signature{ nullptr };
        ID3D12PipelineState* current_pipeline_state{ nullptr };

        for (UINT i{ 0 }; i < items_count; ++i)
        {
            if (current_root_signature != cache.root_signatures[i])
            {
                current_root_signature = cache.root_signatures[i];
                cmd_list->SetGraphicsRootSignature(current_root_signature);
                cmd_list->SetGraphicsRootConstantBufferView(content::opaque_root_parameter::global_shader_data, d3d12_info.global_shader_data);
            }

            if (current_pipeline_state != cache.depth_pipeline_states[i])
            {
                current_pipeline_state = cache.depth_pipeline_states[i];
                cmd_list->SetPipelineState(current_pipeline_state);
            }

            set_root_parameters(cmd_list, i);

            const D3D12_INDEX_BUFFER_VIEW& ibv{ cache.index_buffer_views[i] };
            const UINT index_count{ ibv.SizeInBytes >> (ibv.Format == DXGI_FORMAT_R16_UINT ? 1 : 2) };

            cmd_list->IASetIndexBuffer(&ibv);
            cmd_list->IASetPrimitiveTopology(cache.primitive_topologies[i]);
            cmd_list->DrawIndexedInstanced(index_count, 1, 0, 0, 0);
        }
    }

    void render_targets(id3d12_graphics_command_list* cmd_list, const core::d3d12_frame_info& d3d12_info, barriers::resource_barrier& barriers)
    {
        // add_transitions_for_gpass
        barriers.add(graphic_buffer.resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY);
        barriers.add(depth_buffer.resource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        barriers.apply(cmd_list);

        // set_render_targets_for_gpass
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv{ graphic_buffer.rtv(0) };
        const D3D12_CPU_DESCRIPTOR_HANDLE dsv{ depth_buffer.dsv() };
        cmd_list->ClearRenderTargetView(rtv, clear_value, 0, nullptr);
        cmd_list->OMSetRenderTargets(1, &rtv, 0, &dsv);

        // render
        const graphic_cache& cache{ frame_cache };
        const UINT items_count{ cache.size() };
        const UINT frame_index{ d3d12_info.frame_index };
        const UINT light_culling_id{ d3d12_info.light_id };

        ID3D12RootSignature* current_root_signature{ nullptr };
        ID3D12PipelineState* current_pipeline_state{ nullptr };

        for (UINT i{ 0 }; i < items_count; ++i)
        {
            if (current_root_signature != cache.root_signatures[i])
            {
                using idx = content::opaque_root_parameter;

                current_root_signature = cache.root_signatures[i];
                cmd_list->SetGraphicsRootSignature(current_root_signature);
                cmd_list->SetGraphicsRootConstantBufferView(idx::global_shader_data, d3d12_info.global_shader_data);
                cmd_list->SetGraphicsRootShaderResourceView(idx::directional_lights, lights::non_cullable_light_buffer(frame_index));
                cmd_list->SetGraphicsRootShaderResourceView(idx::cullable_lights, lights::cullable_light_buffer(frame_index));
                cmd_list->SetGraphicsRootShaderResourceView(idx::light_grid, lights::light_grid_opaque(light_culling_id, frame_index));
                cmd_list->SetGraphicsRootShaderResourceView(idx::light_index_list, lights::light_index_list_opaque(light_culling_id, frame_index));
            }

            if (current_pipeline_state != cache.graphic_pipeline_states[i])
            {
                current_pipeline_state = cache.graphic_pipeline_states[i];
                cmd_list->SetPipelineState(current_pipeline_state);
            }

            set_root_parameters(cmd_list, i);

            const D3D12_INDEX_BUFFER_VIEW& ibv{ cache.index_buffer_views[i] };
            const UINT index_count{ ibv.SizeInBytes >> (ibv.Format == DXGI_FORMAT_R16_UINT ? 1 : 2) };

            cmd_list->IASetIndexBuffer(&ibv);
            cmd_list->IASetPrimitiveTopology(cache.primitive_topologies[i]);
            cmd_list->DrawIndexedInstanced(index_count, 1, 0, 0, 0);
        }

        // add_transitions_for_post_process
        barriers.add(graphic_buffer.resource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }


    const resource::Render_Target& get_graphic_buffer()
    {
        return graphic_buffer;
    }

    const resource::Depth_Buffer& get_depth_buffer()
    {
        return depth_buffer;
    }
}
