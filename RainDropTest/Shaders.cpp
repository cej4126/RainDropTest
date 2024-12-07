#include <fstream>
#include <filesystem>
#include <unordered_map>

#include "Shaders.h"
#include "Utilities.h"

// NOTE: we wouldn't need to do this if DXC had a NuGet package.
#pragma comment(lib, "../packages/DirectXShaderCompiler/lib/x64/dxcompiler.lib")

using namespace Microsoft::WRL;

namespace shaders {

    namespace {
        constexpr const char* shader_source_path{ "..\\..\\RainDropTest\\" };
        constexpr const char* engine_shader_file{ "./shaders/d3d12/shaders.bin" };

        struct elements_type {
            enum type : UINT {
                position_only = 0x00,
                static_normal = 0x01,
                static_normal_texture = 0x03,
                static_color = 0x04,
                skeletal = 0x08,
                skeletal_color = skeletal | static_color,
                skeletal_normal = skeletal | static_normal,
                skeletal_normal_color = skeletal_normal | static_color,
                skeletal_normal_texture = skeletal | static_normal_texture,
                skeletal_normal_texture_color = skeletal_normal_texture | static_color,
            };
        };

        typedef struct compiled_shader
        {
            static constexpr UINT hash_length{ 16 };
            constexpr UINT64 byte_code_size() const { return m_byte_code_size; }
            constexpr const UINT8* const hash() const { return &m_hash[0]; }
            constexpr const UINT8* const byte_code() const { return &m_byte_code; }
            constexpr const UINT64 buffer_size() const { return sizeof(m_byte_code_size) + hash_length + m_byte_code_size; }
            constexpr static UINT64 buffer_size(UINT64 size) { return sizeof(m_byte_code_size) + hash_length + size; }
        private:
            UINT64 m_byte_code_size;
            UINT8 m_hash[hash_length];
            UINT8 m_byte_code;
        } const* compiled_shader_ptr;;

        // Each element in this array points to an offset withing the shaders blob.
        compiled_shader_ptr engine_shaders[engine_shader::count]{};

        // This is a chunk of memory that contains all compiled engine shaders.
        // The blob is an array of shader byte code consisting of a u64 size and 
        // an array of bytes.
        std::unique_ptr<UINT8[]> engine_shaders_blob{};

        shader_file_info shader_files[]
        {
            { engine_shader::full_screen_triangle_vs,    "FullScreenTriangle.hlsl", "FullScreenTriangleVS",  shader_type::vertex,   L"", L""},
            { engine_shader::post_process_ps,            "PostProcess.hlsl",        "PostProcessPS",         shader_type::pixel,    L"", L""},
            { engine_shader::grid_frustums_cs,           "GridFrustums.hlsl",       "ComputeGridFrustumsCS", shader_type::compute,  L"-D", L"TILE_SIZE=32"},
            { engine_shader::light_culling_cs,           "CullLights.hlsl",         "CullLightsCS",          shader_type::compute,  L"-D", L"TILE_SIZE=32"},
            { engine_shader::n_body_gravity_cs,          "nBodyGravityCS.hlsl",     "CSMain",                shader_type::compute,  L"", L""},
            { engine_shader::particle_draw_vs,           "ParticleDraw.hlsl",       "VSParticleDraw",        shader_type::vertex,   L"", L""},
            { engine_shader::particle_draw_gs,           "ParticleDraw.hlsl",       "GSParticleDraw",        shader_type::geometry, L"", L""},
            { engine_shader::particle_draw_ps,           "ParticleDraw.hlsl",       "PSParticleDraw",        shader_type::pixel,    L"", L""},
            { engine_shader::pixel_shader_ps,            "AppShader.hlsl",          "ShaderPS",              shader_type::pixel,    L"", L""},
            { engine_shader::texture_shader_ps,          "AppShader.hlsl",          "ShaderPS",              shader_type::pixel,    L"-D", L"TEXTURED_MTL=1"},
            { engine_shader::normal_shader_vs,           "AppShader.hlsl",          "ShaderVS",              shader_type::vertex,   L"-D", L"ELEMENTS_TYPE=1"},
            { engine_shader::normal_texture_shader_vs,   "AppShader.hlsl",          "ShaderVS",              shader_type::vertex,   L"-D", L"ELEMENTS_TYPE=3"},
        };

        static_assert(_countof(shader_files) == engine_shader::count);

        std::unordered_map<UINT, UINT>shader_shader_map
        {
           
            { elements_type::static_normal, engine_shader::normal_shader_vs },
            { elements_type::static_normal_texture, engine_shader::normal_texture_shader_vs },
        };
        std::mutex shader_mutex;

        bool compiled_shaders_are_up_to_date()
        {
            if (!std::filesystem::exists(engine_shader_file))
            {
                return false;
            }
         
            auto shaders_compilation_time = std::filesystem::last_write_time(engine_shader_file);

            for (const auto& entity : std::filesystem::directory_iterator{ shader_source_path })
            {
                auto ext = entity.path().extension();
                if (ext == L".hlsl" || ext == L".hlsli")
                {
                    if (entity.last_write_time() > shaders_compilation_time)
                    {
                        return false;
                    }
                }
            }

            return true;
        }

        bool save_compiled_shaders(utl::vector<dxc_compiled_shader>& shaders)
        {
            std::filesystem::path shaders_path{ engine_shader_file };
            std::filesystem::create_directories(shaders_path.parent_path());
            std::ofstream file(engine_shader_file, std::ios::out | std::ios::binary);
            if (!file || !std::filesystem::exists(engine_shader_file))
            {
                file.close();
                return false;
            }

            for (const auto& shader : shaders)
            {
                const D3D12_SHADER_BYTECODE byte_code{ shader.byte_code->GetBufferPointer(), shader.byte_code->GetBufferSize() };
                file.write((char*)&byte_code.BytecodeLength, sizeof(byte_code.pShaderBytecode));
                file.write((char*)&shader.hash.HashDigest[0], _countof(shader.hash.HashDigest));
                file.write((char*)byte_code.pShaderBytecode, byte_code.BytecodeLength);
            }
            file.close();
            return true;
        }

        bool load_engine_shaders()
        {
            assert(!engine_shaders_blob);
            UINT64 size{ 0 };
            bool result{ utl::read_file(engine_shader_file, engine_shaders_blob, size) };
            assert(engine_shaders_blob && size);

            UINT64 offset{ 0 };
            UINT index{ 0 };
            while (offset < size && result)
            {
                assert(index < engine_shader::count);
                compiled_shader_ptr& shader{ engine_shaders[index] };
                assert(!shader);
                result &= index < engine_shader::count && !shader;
                if (!result) break;
                shader = reinterpret_cast<const compiled_shader_ptr>(&engine_shaders_blob[offset]);
                offset += shader->buffer_size();
                ++index;
            }
            assert(offset == size && index == engine_shader::count);

            return result;
        }

    } // anonymous namespace

    shader_compiler::shader_compiler()
    {
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_compiler)));
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_utils)));
        ThrowIfFailed(m_utils->CreateDefaultIncludeHandler(&m_include_handler));
    }

    dxc_compiled_shader shader_compiler::compile(shader_file_info info)
    {
        assert(m_compiler && m_utils && m_include_handler);

        // Load the source file using Utils interface
        std::filesystem::path full_path = shader_source_path;
        full_path += info.file_name;
        ComPtr<IDxcBlobEncoding> source_blob{ nullptr };
        HRESULT hr{ S_OK };
        hr = m_utils->LoadFile(full_path.c_str(), nullptr, &source_blob);
        if (FAILED(hr))
        {
            return {};
        }

        //ThrowIfFailed(m_utils->LoadFile(full_path.c_str(), nullptr, &source_blob));
        assert(source_blob && source_blob->GetBufferSize());

        OutputDebugStringA("Compiling ");
        OutputDebugStringA(info.file_name);
        OutputDebugStringA(" : ");
        OutputDebugStringA(info.function);
        OutputDebugStringA("\n");

        return compile(source_blob.Get(), get_args(info));
    }

    dxc_compiled_shader shader_compiler::compile(IDxcBlobEncoding* source_blob, utl::vector<std::wstring> compiler_args)
    {
        DxcBuffer buffer{};
        buffer.Encoding = DXC_CP_ACP; // auto-detect text format?
        buffer.Ptr = source_blob->GetBufferPointer();
        buffer.Size = source_blob->GetBufferSize();

        utl::vector<LPCWSTR> args;
        for (const auto& arg : compiler_args)
        {
            args.emplace_back(arg.c_str());
        }

        ComPtr<IDxcResult> results{ nullptr };
        ThrowIfFailed(m_compiler->Compile(&buffer, args.data(), (UINT)args.size(), m_include_handler.Get(), IID_PPV_ARGS(&results)));
        ComPtr<IDxcBlobUtf8> errors{ nullptr };
        ThrowIfFailed(results->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr));

        if (errors && errors->GetStringLength())
        {
            OutputDebugStringA("\nShader compilation error: \n");
            OutputDebugStringA(errors->GetStringPointer());
        }
        else
        {
            OutputDebugStringA(" [ Succeeded ]");
        }
        OutputDebugStringA("\n");

        HRESULT status{ S_OK };
        ThrowIfFailed(results->GetStatus(&status));
        if (FAILED(status)) return {};

        ComPtr<IDxcBlob> hash{ nullptr };
        ThrowIfFailed(results->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(&hash), nullptr));
        DxcShaderHash* const hash_buffer{ (DxcShaderHash* const)hash->GetBufferPointer() };
        // different source code could result in the same byte code, so we only care about byte code hash.
        assert(!(hash_buffer->Flags & DXC_HASHFLAG_INCLUDES_SOURCE));
        OutputDebugStringA("Shader hash: ");
        for (UINT i{ 0 }; i < _countof(hash_buffer->HashDigest); ++i)
        {
            char hash_bytes[3]{}; // 2 chars for hex value plus termination 0.
            sprintf_s(hash_bytes, "%02x", (UINT)hash_buffer->HashDigest[i]);
            OutputDebugStringA(hash_bytes);
            OutputDebugStringA(" ");
        }
        OutputDebugStringA("\n");

        ComPtr<IDxcBlob> shader{ nullptr };
        ThrowIfFailed(results->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader), nullptr));
        buffer.Ptr = shader->GetBufferPointer();
        buffer.Size = shader->GetBufferSize();

        ComPtr<IDxcResult> disasm_results{ nullptr };
        ThrowIfFailed(m_compiler->Disassemble(&buffer, IID_PPV_ARGS(&disasm_results)));

        ComPtr<IDxcBlobUtf8> disassembly{ nullptr };
        ThrowIfFailed(disasm_results->GetOutput(DXC_OUT_DISASSEMBLY, IID_PPV_ARGS(&disassembly), nullptr));

        dxc_compiled_shader result{ shader.Detach(), disassembly.Detach() };
        memcpy(&result.hash.HashDigest[0], &hash_buffer->HashDigest[0], _countof(hash_buffer->HashDigest));

        return result;
    }

    utl::vector<std::wstring> shader_compiler::get_args(const shader_file_info& info)
    {
        utl::vector<std::wstring> args{};

        args.emplace_back(utl::to_wstring(info.file_name));
        args.emplace_back(L"-E");
        args.emplace_back(utl::to_wstring(info.function));
        args.emplace_back(L"-T");
        args.emplace_back(utl::to_wstring(m_profile_strings[(UINT)info.type]));
        args.emplace_back(L"-I");
        args.emplace_back(utl::to_wstring(shader_source_path));
        args.emplace_back(L"-enable-16bit-types");
        args.emplace_back(DXC_ARG_ALL_RESOURCES_BOUND);
#ifdef _DEBUG
        args.emplace_back(DXC_ARG_DEBUG);
        args.emplace_back(DXC_ARG_SKIP_OPTIMIZATIONS);
#else
        args.emplace_back(DXC_ARG_OPTIMIZATION_LEVEL3);
#endif // _DEBUG
        args.emplace_back(DXC_ARG_WARNINGS_ARE_ERRORS);
        args.emplace_back(L"-Qstrip_reflect");                               // Strip reflections into a separate blob
        args.emplace_back(L"-Qstrip_debug");                                 // Strip debug information into a separate blob
        if (info.arg1.size())
        {
            args.emplace_back(info.arg1);
        }
        if (info.arg2.size())
        {
            args.emplace_back(info.arg2);
        }

        return args; 
    }

    bool initialize()
    {
        std::lock_guard lock{ shader_mutex };

        return load_engine_shaders();
    }

    void shutdown()
    {
        std::lock_guard lock{ shader_mutex };

        for (UINT i{ 0 }; i < engine_shader::count; ++i)
        {
            engine_shaders[i] = {};
        }
        engine_shaders_blob.reset();
    }

    UINT element_type_to_shader_id(UINT key)
    {
        std::lock_guard lock{ shader_mutex };
        auto pair = shader_shader_map.find(key);
        if (pair == shader_shader_map.end())
        {
            throw;
        }
        return pair->second;
    }

    D3D12_SHADER_BYTECODE get_engine_shader(UINT id)
    {
        std::lock_guard lock{ shader_mutex };
        assert(id < engine_shader::count);
        const compiled_shader_ptr& shader{ engine_shaders[id] };
        assert(shader && shader->byte_code_size());
        return { shader->byte_code(), shader->byte_code_size() };
    }


    // ShaderCompilation.cpp
    bool compile_shaders()
    {
        std::lock_guard lock{ shader_mutex };
        if (compiled_shaders_are_up_to_date())
        {
            return true;
        }

        shader_compiler compiler{};
        utl::vector<dxc_compiled_shader> shaders;
        std::filesystem::path full_path{};

        // compile shaders and them together in a buffer in the same order of compilation.
        for (UINT i{ 0 }; i < engine_shader::count; ++i)
        {
            auto& file = shader_files[i];

            dxc_compiled_shader compiled_shader{ compiler.compile(file) };
            if (compiled_shader.byte_code && compiled_shader.byte_code->GetBufferPointer() && compiled_shader.byte_code->GetBufferSize())
            {
                shaders.emplace_back(std::move(compiled_shader));
            }
            else
            {
                return false;
            }
        }

        return save_compiled_shaders(shaders);
    }

}