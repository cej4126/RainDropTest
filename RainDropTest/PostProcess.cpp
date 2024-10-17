#include "PostProcess.h"
#include "Core.h"
#include "Helpers.h"
#include "Shaders.h"
#include "GraphicPass.h"
#include "Lights.h"

namespace post_process {

    namespace {

        struct root_param_indices
        {
            enum param : UINT {
                global_shader_data,
                root_constants,

                // TODO: temporary for visualizing light culling. Remove later.
                frustums,
                light_grid_opaque,

                count
            };
        };

        ID3D12RootSignature* fx_root_signature{ nullptr };
        ID3D12PipelineState* fx_pso{ nullptr };

        void create_root_signature_pso()
        {
            // root signature
            assert(!fx_root_signature && !fx_pso);

            using idx = root_param_indices;
            d3dx::d3d12_root_parameter parameters[idx::count]{};
            parameters[idx::global_shader_data].as_cbv(D3D12_SHADER_VISIBILITY_PIXEL, 0);
            parameters[idx::root_constants].as_constants(1, D3D12_SHADER_VISIBILITY_PIXEL, 1);
            parameters[idx::frustums].as_srv(D3D12_SHADER_VISIBILITY_PIXEL, 0);
            parameters[idx::light_grid_opaque].as_srv(D3D12_SHADER_VISIBILITY_PIXEL, 1);

            d3dx::d3d12_root_signature_desc root_signature_desc{ &parameters[0], _countof(parameters) };
            root_signature_desc.Flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
            fx_root_signature = root_signature_desc.create();
            assert(fx_root_signature);
            NAME_D3D12_OBJECT(fx_root_signature, L"Post Process Root Signature");

            // pipeline state object
            struct {
                d3dx::d3d12_pipeline_state_subobject_root_signature root_signature{ fx_root_signature };
                d3dx::d3d12_pipeline_state_subobject_vs vs{ shaders::get_engine_shader(shaders::engine_shader::full_screen_triangle_vs) };
                d3dx::d3d12_pipeline_state_subobject_ps ps{ shaders::get_engine_shader(shaders::engine_shader::post_process_ps) };
                d3dx::d3d12_pipeline_state_subobject_primitive_topology primitive_topology{ D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE };
                d3dx::d3d12_pipeline_state_subobject_render_target_formats render_target_formats;
                d3dx::d3d12_pipeline_state_subobject_rasterizer resterized{ d3dx::rasterizer_state.no_cull };
            } stream;

            D3D12_RT_FORMAT_ARRAY rtf_array{};
            rtf_array.NumRenderTargets = 1;
            rtf_array.RTFormats[0] = surface::Surface::default_back_buffer_format;
            stream.render_target_formats = rtf_array;

            fx_pso = d3dx::create_pipeline_state(&stream, sizeof(stream));
            NAME_D3D12_OBJECT(fx_pso, L"Post Process Pipeline State Object");
            assert(fx_pso);
        }

    } // anonymous namespace

    bool initialize()
    {
        create_root_signature_pso();
        return true;
    }

    void shutdown()
    {
        core::release(fx_root_signature);
        core::release(fx_pso);
    }

    void post_process(id3d12_graphics_command_list* cmd_list, const core::d3d12_frame_info& info, D3D12_CPU_DESCRIPTOR_HANDLE target_rtv)
    {
        const UINT frame_index{ info.frame_index };
        const UINT light_culling_id{ info.light_id };

        cmd_list->SetGraphicsRootSignature(fx_root_signature);
        cmd_list->SetPipelineState(fx_pso);

        using idx = root_param_indices;
        cmd_list->SetGraphicsRootConstantBufferView(idx::global_shader_data, info.global_shader_data);
        cmd_list->SetGraphicsRoot32BitConstant(idx::root_constants, graphic_pass::get_graphic_buffer().srv().index, 0);
        cmd_list->SetGraphicsRootShaderResourceView(idx::frustums, lights::frustums(light_culling_id, frame_index));
        cmd_list->SetGraphicsRootShaderResourceView(idx::light_grid_opaque, lights::light_grid_opaque(light_culling_id, frame_index));

        // NOTE: we don't need to clear the render target, because each pixel will 
        //       be overwritten by pixels from gpass main buffer.
        //       We also don't need a depth buffer.
        cmd_list->OMSetRenderTargets(1, &target_rtv, 1, nullptr);
        cmd_list->DrawInstanced(3, 1, 0, 0);
    }
}
