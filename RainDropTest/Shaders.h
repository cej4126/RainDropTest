#pragma once
#include "stdafx.h"
#include "Vector.h"
#include "../packages/DirectXShaderCompiler/inc/d3d12shader.h"
#include "../packages/DirectXShaderCompiler/inc/dxcapi.h"

namespace shaders {
    struct shader_type {
        enum type : UINT {
            vertex = 0,
            hull,
            domain,
            geometry,
            pixel,
            compute,
            amplification,
            mesh,

            count
        };
    };

    struct shader_flags {
        enum flags : UINT {
            none          = 0x00,
            vertex        = 0x01,
            hull          = 0x02,
            domain        = 0x04,
            geometry      = 0x08,
            pixel         = 0x10,
            compute       = 0x20,
            amplification = 0x40,
            mesh          = 0x80,
        };
    };

    struct engine_shader {
        enum index : UINT {
            vertex_shader_vs,
            //full_screen_triangle_vs,
            //post_process_ps,
            //grid_frustums_cs,
            //light_culling_cs,
            pixel_shader_ps,
            //normal_shader_vs,
            //normal_texture_shader_vs,
            n_body_gravity_cs,
            particle_draw_vs,
            particle_draw_gs,
            particle_draw_ps,

            count
        };
    };

    struct shader_file_info
    {
        UINT shader_index;
        const char* file_name;
        const char* function;
        shader_type::type type;
        std::wstring arg;
    };

    struct dxc_compiled_shader
    {
        ComPtr<IDxcBlob> byte_code;
        ComPtr<IDxcBlobUtf8> disassembly;
        DxcShaderHash hash;
    };

    class shader_compiler
    {
    public:
        shader_compiler();
        DISABLE_COPY_AND_MOVE(shader_compiler);

        dxc_compiled_shader compile(shader_file_info info);

        dxc_compiled_shader compile(IDxcBlobEncoding* source_blob, utl::vector<std::wstring> compiler_args);
    private:
        utl::vector<std::wstring> get_args(const shader_file_info& info);

        // NOTE: Shader Model 6.x can also be used (AS and MS are only supported from SM6.5 on).
        constexpr static const char* m_profile_strings[]{ "vs_6_6", "hs_6_6", "ds_6_6", "gs_6_6", "ps_6_6", "cs_6_6", "as_6_6", "ms_6_6" };
        static_assert(_countof(m_profile_strings) == shader_type::count);

        ComPtr<IDxcCompiler3> m_compiler{ nullptr };
        ComPtr<IDxcUtils> m_utils{ nullptr };
        ComPtr<IDxcIncludeHandler> m_include_handler{ nullptr };
    };

    bool initialize();
    void shutdown();
    UINT element_type_to_shader_id(UINT key);
    D3D12_SHADER_BYTECODE get_engine_shader(UINT id);

    std::unique_ptr<UINT8[]> compile_shaders(shader_file_info info);
    bool compile_shaders();

}
