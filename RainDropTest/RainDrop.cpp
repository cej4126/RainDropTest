#include "stdafx.h"
#include "RainDrop.h"
#include "Command.h"
#include "Main.h"
#include "Barriers.h"
#include "Upload.h"
#include "Buffers.h"

// InterlockedCompareExchange returns the object's value if the 
// comparison fails.  If it is already 0, then its value won't 
// change and 0 will be returned.
#define InterlockedGetValue(object) InterlockedCompareExchange(object, 0, 0)

namespace d3d12 {

    namespace {
        Depth_Buffer m_depth_buffer{};

    } // anonymous namespace


    //const float RainDrop::Particle_Spread = 400.0f;
    const float RainDrop::Particle_Spread = 500.0f;

    RainDrop::RainDrop(UINT width, UINT height, std::wstring name) :
        DXSample(width, height, name),
        m_frame_index(0),
        m_srv_uav_descriptor_size(0),
        m_render_context_fence_value(0),
        m_terminating(0)
    {
        srand(0);

        m_srv_index = 0;
        ZeroMemory(m_frame_fence_values, sizeof(m_frame_fence_values));

        m_render_context_fence_value1 = 0;
        m_thread_fence_value = 0;

        //float sqRootNumAsyncContexts = (float)sqrt(static_cast<float>(Thread_Count));
        //m_height_instances = static_cast<UINT>(ceil(sqRootNumAsyncContexts));
        //m_width_instances = static_cast<UINT>(ceil(sqRootNumAsyncContexts));
        //
        //if (m_width_instances * (m_height_instances - 1) >= Thread_Count)
        //{
        //    --m_height_instances;
        //}
    }

    void RainDrop::OnInit()
    {
        m_camera.Init({ 0.0f, 0.0f, 1500.0f });
        m_camera.SetMoveSpeed(250.0f);

        LoadPipeline();
        LoadAssets();
        CreateAsyncContexts();
    }

    // Load the rendering pipeline dependencies.
    void RainDrop::LoadPipeline()
    {
#if defined(_DEBUG)
        // Enable the D3D12 debug layer.
        {
            ComPtr<ID3D12Debug> debug_controller;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
            {
                debug_controller->EnableDebugLayer();
            }
        }
#endif

    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_factory)));

    if (m_use_warp_device)
    {
        // Wrap device
        ComPtr<IDXGIAdapter> warp_adapter;
        ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter)));
        ThrowIfFailed(D3D12CreateDevice(warp_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardware_adapter;
        GetHardwareAdapter(m_factory.Get(), &hardware_adapter);

        ThrowIfFailed(D3D12CreateDevice(hardware_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
    }

    new (&m_command) command::Command(m_device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
    if (!m_command.command_queue())
    {
        throw;
    }

    bool result{ true };
    result &= m_rtv_desc_heap.initialize(512, false);
    result &= m_dsv_desc_heap.initialize(512, false);
    result &= m_srv_desc_heap.initialize(4096, true);
    result &= m_uav_desc_heap.initialize(512, false);
    if (!result)
    {
        throw;
    }

    NAME_D3D12_OBJECT(m_rtv_desc_heap.heap(), L"RTV Descriptor Heap");
    NAME_D3D12_OBJECT(m_dsv_desc_heap.heap(), L"DSV Descriptor Heap");
    NAME_D3D12_OBJECT(m_srv_desc_heap.heap(), L"SRV Descriptor Heap");
    NAME_D3D12_OBJECT(m_uav_desc_heap.heap(), L"UAV Descriptor Heap");

    // descriptor heap
    {
        // Describe and create a shader resource view (SRV) and unordered
        // access view (UAV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC srv_uav_heap_desc = {};
        srv_uav_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;      // D3D12_DESCRIPTOR_HEAP_TYPE Type;
        srv_uav_heap_desc.NumDescriptors = Descriptor_Count;                  // UINT NumDescriptors;
        srv_uav_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;    // D3D12_DESCRIPTOR_HEAP_FLAGS Flags;
        srv_uav_heap_desc.NodeMask = 0;                        // UINT NodeMask;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&srv_uav_heap_desc, IID_PPV_ARGS(&m_srv_uav_heap)));
        NAME_D3D12_COMPTR_OBJECT(m_srv_uav_heap);

        m_srv_uav_descriptor_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    if (!(upload::initialize()))
    {
        throw;
    }
    }

    void RainDrop::LoadAssets()
    {
        // Create the root signatures.
        {
            d3dx::d3d12_descriptor_range range_srv{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0 };
            d3dx::d3d12_descriptor_range range_uav{ D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0 };

            d3dx::d3d12_root_parameter root_parameters[Root_Parameters_Count]{};
            // Constant Buffer
            root_parameters[Root_Parameter_CB].as_cbv(D3D12_SHADER_VISIBILITY_ALL, 0);
            // SRV Descriptor Table
            root_parameters[Root_Parameter_SRV].as_descriptor_table(D3D12_SHADER_VISIBILITY_VERTEX, &range_srv, 1);
            // UAV Descriptor Table
            root_parameters[Root_Parameter_UAV].as_descriptor_table(D3D12_SHADER_VISIBILITY_ALL, &range_uav, 1);

            d3dx::d3d12_root_signature_desc root_signature_desc{ &root_parameters[0], Root_Parameter_SRV + 1,
                 D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };

            m_root_signature.Attach(root_signature_desc.create());
            assert(m_root_signature.Get());
            NAME_D3D12_OBJECT(m_root_signature.Get(), L"Root Signature");

            // Create compute root signature
            root_parameters[Root_Parameter_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            d3dx::d3d12_root_signature_desc compute_root_signature_desc{ &root_parameters[0], Root_Parameters_Count, D3D12_ROOT_SIGNATURE_FLAG_NONE };
            m_compute_root_signature.Attach(compute_root_signature_desc.create());
            assert(m_compute_root_signature.Get());
            NAME_D3D12_OBJECT(m_compute_root_signature.Get(), L"Compute Root Signature");
        }

        // Create the pipeline states, which includes compiling and loading shaders.
        {
            ComPtr<ID3DBlob> vertex_shader;
            ComPtr<ID3DBlob> geometry_shader;
            ComPtr<ID3DBlob> pixel_shader;
            ComPtr<ID3DBlob> compute_shader;

#if defined(_DEBUG)
            UINT compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
            UINT computeFlags = 0;
#endif
            ComPtr<ID3DBlob> pErrorMsgs{ nullptr };


            struct shader_file_info
            {
                const std::wstring file_name;
                const std::string function;
                const std::string compiler;
                ComPtr<ID3DBlob> shaders;
            };
            shader_file_info shader_file_infos[]
            {
               { L"ParticleDraw.hlsl",   "VSParticleDraw", "vs_5_0" },
               { L"ParticleDraw.hlsl",   "GSParticleDraw", "gs_5_0" },
               { L"ParticleDraw.hlsl",   "PSParticleDraw", "ps_5_0" },
               { L"NBodyGravityCS.hlsl", "CSMain",         "cs_5_0" }
            };

            //ComPtr<ID3DBlob> shaders[4];

            for (UINT i = 0; i < _countof(shader_file_infos); ++i)
            {
                if (FAILED(D3DCompileFromFile(GetAssetFullPath(shader_file_infos[i].file_name.c_str()).c_str(), nullptr, nullptr, shader_file_infos[i].function.c_str(),
                    shader_file_infos[i].compiler.c_str(), compile_flags, 0, &shader_file_infos[i].shaders, &pErrorMsgs)))
                {
                    const char* error_msg{ pErrorMsgs ? (const char*)pErrorMsgs->GetBufferPointer() : "" };
                    OutputDebugStringA(error_msg);
                }
            }
            vertex_shader = shader_file_infos[0].shaders;
            geometry_shader = shader_file_infos[1].shaders;
            pixel_shader = shader_file_infos[2].shaders;
            compute_shader = shader_file_infos[3].shaders;

            D3D12_INPUT_ELEMENT_DESC input_element_descriptors[] =
            {
                { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            };

            // Describe and create the graphics pipeline state object (PSO).
            D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
            pso_desc.pRootSignature = m_root_signature.Get();                                              // ID3D12RootSignature* pRootSignature;
            pso_desc.VS = { vertex_shader->GetBufferPointer(), vertex_shader->GetBufferSize() };           // D3D12_SHADER_BYTECODE VS;
            pso_desc.PS = { pixel_shader->GetBufferPointer(), pixel_shader->GetBufferSize() };             // D3D12_SHADER_BYTECODE PS;
            pso_desc.DS = {};                                                                              // D3D12_SHADER_BYTECODE DS;
            pso_desc.HS = {};                                                                              // D3D12_SHADER_BYTECODE HS;
            pso_desc.GS = { geometry_shader->GetBufferPointer(), geometry_shader->GetBufferSize() };       // D3D12_SHADER_BYTECODE GS;
            pso_desc.StreamOutput = {};                                                                    // D3D12_STREAM_OUTPUT_DESC StreamOutput;
            pso_desc.BlendState = d3dx::blend_state.blend_desc;                                                  // D3D12_BLEND_DESC BlendState;
            pso_desc.SampleMask = UINT_MAX;                                                                // UINT SampleMask;
            pso_desc.RasterizerState = d3dx::rasterizer_state.face_cull;                                         // D3D12_RASTERIZER_DESC RasterizerState;
            pso_desc.DepthStencilState = d3dx::depth_desc_state.enable;                                          // D3D12_DEPTH_STENCIL_DESC DepthStencilState;
            pso_desc.InputLayout = { input_element_descriptors, _countof(input_element_descriptors) };     // D3D12_INPUT_LAYOUT_DESC InputLayout;
            pso_desc.IBStripCutValue = {};                                                                 // D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
            pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;                          // D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType;
            pso_desc.NumRenderTargets = 1;                                                                 // UINT NumRenderTargets;
            pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;                                           // DXGI_FORMAT RTVFormats[8];
            pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;                                                    // DXGI_FORMAT DSVFormat;
            pso_desc.SampleDesc = { 1, 0 };                                                                // DXGI_SAMPLE_DESC SampleDesc;
            pso_desc.NodeMask = 0;                                                                         // UINT NodeMask;
            pso_desc.CachedPSO = {};                                                                       // D3D12_CACHED_PIPELINE_STATE CachedPSO;
            pso_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;                                               // D3D12_PIPELINE_STATE_FLAGS Flags;

            ThrowIfFailed(m_device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&m_pipeline_state)));
            NAME_D3D12_COMPTR_OBJECT(m_pipeline_state);

            // Describe and create the compute pipeline state object (PSO).
            D3D12_COMPUTE_PIPELINE_STATE_DESC compute_pso_desc = {};
            compute_pso_desc.pRootSignature = m_compute_root_signature.Get();                              // ID3D12RootSignature* pRootSignature;
            compute_pso_desc.CS = { compute_shader->GetBufferPointer(), compute_shader->GetBufferSize() }; // D3D12_SHADER_BYTECODE CS;
            compute_pso_desc.NodeMask = 0;                                                                 // UINT NodeMask;
            compute_pso_desc.CachedPSO = {};                                                               // D3D12_CACHED_PIPELINE_STATE CachedPSO;
            compute_pso_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;                                       // D3D12_PIPELINE_STATE_FLAGS Flags;

            ThrowIfFailed(m_device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&m_compute_state)));
            NAME_D3D12_COMPTR_OBJECT(m_compute_state);

        }

        CreateVertexBuffer();
        CreateParticleBuffers();

        // Note: Common_Pointer's are CPU objects but this resource needs to stay in scope until
        // the command list that references it has finished executing on the GPU.
        // We will flush the GPU at the end of this method to ensure the resource is not
        // prematurely destroyed.
        // Other words-  Complete the GPU work before ending the routine and auto removing the constantBufferCSUpload.
        ComPtr<ID3D12Resource> constant_buffer_cs_upload;

        // Create the compute shader's constant buffer.
        {
            const UINT buffer_size = sizeof(Constant_Buffer_CS);

            Constant_Buffer_CS constant_buffer_cs = {};
            constant_buffer_cs.param[0] = Particle_Count;
            constant_buffer_cs.param[1] = int(ceil(Particle_Count / 128.0f));
            constant_buffer_cs.param_float[0] = 0.1f;
            constant_buffer_cs.param_float[1] = 1.0f;

            //m_command.command_list()->Close();
            m_constant_buffer_cs.Attach(buffers::create_buffer_default_with_upload(&constant_buffer_cs, buffer_size));
        }

        // Create the geometry shader's constant buffer.
        {
            const UINT constant_buffer_gs_size = sizeof(Constant_Buffer_GS) * Frame_Count;

            D3D12_RESOURCE_DESC buffer_desc = {};
            buffer_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;        // D3D12_RESOURCE_DIMENSION Dimension;
            buffer_desc.Alignment = 0;                                      // UINT64 Alignment;
            buffer_desc.Width = constant_buffer_gs_size;                    // UINT64 Width;
            buffer_desc.Height = 1;                                         // UINT Height;
            buffer_desc.DepthOrArraySize = 1;                               // UINT16 DepthOrArraySize;
            buffer_desc.MipLevels = 1;                                      // UINT16 MipLevels;
            buffer_desc.Format = DXGI_FORMAT_UNKNOWN;                       // DXGI_FORMAT Format;
            buffer_desc.SampleDesc = { 1, 0 };                              // DXGI_SAMPLE_DESC SampleDesc;
            buffer_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;            // D3D12_TEXTURE_LAYOUT Layout;
            buffer_desc.Flags = D3D12_RESOURCE_FLAG_NONE;                   // D3D12_RESOURCE_FLAGS Flags;

            ThrowIfFailed(m_device->CreateCommittedResource(&d3dx::heap_properties.upload_heap, D3D12_HEAP_FLAG_NONE, &buffer_desc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_constant_buffer_gs)));
            NAME_D3D12_COMPTR_OBJECT(m_constant_buffer_gs);

            D3D12_RANGE read_range = {}; // We do not intend to read from this resource on the CPU.
            read_range.Begin = 0;
            read_range.End = 0;
            ThrowIfFailed(m_constant_buffer_gs->Map(0, &read_range, reinterpret_cast<void**>(&m_p_constant_buffer_gs_data)));
            ZeroMemory(m_p_constant_buffer_gs_data, constant_buffer_gs_size);
        }

        // Create the depth stencil view.
        assert(m_width && m_height);
        m_depth_buffer.release();

        m_depth_buffer = Depth_Buffer{ m_width, m_height };

        // Close the command list and execute it to begin the initial GPU setup and uploads.
        ThrowIfFailed(m_command.command_list()->Close());
        ID3D12CommandList* pp_command_lists[] = { m_command.command_list() };
        m_command.command_queue()->ExecuteCommandLists(_countof(pp_command_lists), pp_command_lists);

        // Create synchronization objects and wait until assets have been uploaded to the GPU.
        {
            ThrowIfFailed(m_device->CreateFence(m_render_context_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_render_context_fence)));
            ++m_render_context_fence_value;

            m_render_context_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (m_render_context_fence_event == nullptr)
            {
                ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
            }

            WaitForRenderContext();
        }
    }

    // Create the particle vertex buffer.
    void RainDrop::CreateVertexBuffer()
    {
        std::vector<Particle_Vertex> vertices;
        vertices.resize(Particle_Count);
        for (UINT i = 0; i < Particle_Count; ++i)
        {
            if ((i % 3) == 0)
            {
                vertices[i].color = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
            }
            else if ((i % 3) == 1)
            {
                vertices[i].color = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
            }
            else
            {
                vertices[i].color = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
            }
            //vertices[i].color = XMFLOAT4(1.0f, 1.0f, 0.2f, 1.0f);
        }

        const UINT buffer_size = Particle_Count * sizeof(Particle_Vertex);

        //m_command.command_list()->Close();
        m_vertex_buffer.Attach(buffers::create_buffer_default_with_upload(&vertices[0], buffer_size));

        m_vertex_buffer_view.BufferLocation = m_vertex_buffer->GetGPUVirtualAddress();
        m_vertex_buffer_view.SizeInBytes = static_cast<UINT>(buffer_size);
        m_vertex_buffer_view.StrideInBytes = sizeof(Particle_Vertex);
    }

    // Random percent value, from -1 to 1.
    float RainDrop::RandomPercent()
    {
        int r = rand();
        float ret = static_cast<float>((r % 10000) - 5000);
        return ret / 5000.0f;
    }

    void RainDrop::LoadParticles(Particle* p_particles, const XMFLOAT3& center, const float& velocity, float spread, UINT number_of_particles)
    {
        const float pi_2 = 3.141592654f;

        for (UINT i = 0; i < number_of_particles; ++i)
        {
            XMFLOAT3 delta(spread, spread, spread);

            p_particles[i].position.x = center.x + RandomPercent() * spread;
            p_particles[i].position.y = 1000.0f + (RandomPercent()) * 500.f;
            p_particles[i].position.z = center.z + RandomPercent() * spread;
            p_particles[i].position.w = p_particles[i].position.y;

            //p_particles[i].velocity = velocity;
            p_particles[i].velocity.x = RandomPercent() * 0.0005f;
            p_particles[i].velocity.y = -0.01f + RandomPercent() * 0.005f;
            p_particles[i].velocity.z = RandomPercent() * 0.0005f;
        }
    }

    // Create the position and velocity buffer shader resources.
    void RainDrop::CreateParticleBuffers()
    {
        // Initialize the data in the buffers
        std::vector<Particle> data;
        data.resize(Particle_Count);
        const UINT data_size = Particle_Count * sizeof(Particle);

        // Split the particles into two groups.
        float center_spread = 1000.0f;
        float speed = 0.01f;

        const UINT number_of_objects = 5;
        // center             vel                   spread
        LoadParticles(&data[0], XMFLOAT3(0, 0, 0), speed, center_spread, Particle_Count - number_of_objects);

        speed = 1.0f;
        center_spread = 1000.f;
        const UINT start_index = Particle_Count - number_of_objects;
        LoadParticles(&data[start_index], XMFLOAT3(0, 0, 0), speed, center_spread, number_of_objects);

        //m_command.command_list()->Close();
        m_particle_buffer_0.Attach(buffers::create_buffer_default_with_upload(&data[0], data_size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS));
        m_particle_buffer_1.Attach(buffers::create_buffer_default_with_upload(&data[0], data_size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS));

        // Create the resource heap view for the srv
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // UINT Shader4ComponentMapping;
        srv_desc.Format = DXGI_FORMAT_UNKNOWN;                                       // DXGI_FORMAT Format;                  
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;                         // D3D12_SRV_DIMENSION ViewDimension;
        srv_desc.Buffer.FirstElement = 0;                                            // UINT64 FirstElement;
        srv_desc.Buffer.NumElements = Particle_Count;                                // UINT NumElements;
        srv_desc.Buffer.StructureByteStride = sizeof(Particle);                      // UINT StructureByteStride;
        srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;                          // D3D12_BUFFER_SRV_FLAGS Flags;

        D3D12_CPU_DESCRIPTOR_HANDLE srv_handle_0 = m_srv_uav_heap->GetCPUDescriptorHandleForHeapStart();
        srv_handle_0.ptr += (SIZE_T)((Srv_Particle_Pos_Vel_0)*m_srv_uav_descriptor_size);
        D3D12_CPU_DESCRIPTOR_HANDLE srv_handle_1 = m_srv_uav_heap->GetCPUDescriptorHandleForHeapStart();
        srv_handle_1.ptr += (SIZE_T)((Srv_Particle_Pos_Vel_1)*m_srv_uav_descriptor_size);

        m_device->CreateShaderResourceView(m_particle_buffer_0.Get(), &srv_desc, srv_handle_0);
        m_device->CreateShaderResourceView(m_particle_buffer_1.Get(), &srv_desc, srv_handle_1);

        // Create the resource heap view for the uav

        // Create the resource heap view for the srv
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
        uav_desc.Format = DXGI_FORMAT_UNKNOWN;                                       // DXGI_FORMAT Format;                  
        uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;                         // D3D12_SRV_DIMENSION ViewDimension;
        uav_desc.Buffer.FirstElement = 0;                                            // UINT64 FirstElement;
        uav_desc.Buffer.NumElements = Particle_Count;                                // UINT NumElements;
        uav_desc.Buffer.StructureByteStride = sizeof(Particle);                      // UINT StructureByteStride;
        uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;                          // D3D12_BUFFER_SRV_FLAGS Flags;

        D3D12_CPU_DESCRIPTOR_HANDLE uav_handle_0 = m_srv_uav_heap->GetCPUDescriptorHandleForHeapStart();
        uav_handle_0.ptr += (SIZE_T)((Uav_Particle_Pos_Vel_0)*m_srv_uav_descriptor_size);
        D3D12_CPU_DESCRIPTOR_HANDLE uav_handle_1 = m_srv_uav_heap->GetCPUDescriptorHandleForHeapStart();
        uav_handle_1.ptr += (SIZE_T)((Uav_Particle_Pos_Vel_1)*m_srv_uav_descriptor_size);

        m_device->CreateUnorderedAccessView(m_particle_buffer_0.Get(), nullptr, &uav_desc, uav_handle_0);
        m_device->CreateUnorderedAccessView(m_particle_buffer_1.Get(), nullptr, &uav_desc, uav_handle_1);
    }

    void RainDrop::CreateAsyncContexts()
    {
        // Create the compute resources.
        D3D12_COMMAND_QUEUE_DESC queue_desc = { D3D12_COMMAND_LIST_TYPE_COMPUTE, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0 };
        ThrowIfFailed(m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_compute_command_queue)));
        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_compute_command_allocator)));
        ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_compute_command_allocator.Get(), nullptr, IID_PPV_ARGS(&m_compute_command_list)));
        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_thread_fence)));

        m_thread_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_thread_fence_event == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        m_thread_data.p_context = this;
        m_thread_data.thread_index = 0;

        m_thread_handle = CreateThread(nullptr, 0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(ThreadProc),
            reinterpret_cast<void*>(&m_thread_data),
            CREATE_SUSPENDED, nullptr);

        ResumeThread(m_thread_handle);
    }

    // Update frame-based values.
    void RainDrop::OnUpdate(float dt)
    {
        // Wait for the previous Present to complete.
        WaitForSingleObjectEx(m_surface.swap_chain_event(), 100, FALSE);

        m_camera.Update(dt);

        Constant_Buffer_GS constant_buffer_gs = {};
        XMStoreFloat4x4(&constant_buffer_gs.world_view_projection, XMMatrixMultiply(m_camera.GetViewMatrix(), m_camera.GetProjectionMatrix(0.8f, m_aspect_ratio, 1.0f, 5000.0f)));
        XMStoreFloat4x4(&constant_buffer_gs.inverse_view, XMMatrixInverse(nullptr, m_camera.GetViewMatrix()));

        UINT8* destination = m_p_constant_buffer_gs_data + sizeof(Constant_Buffer_GS) * m_frame_index;
        memcpy(destination, &constant_buffer_gs, sizeof(Constant_Buffer_GS));
    }

    // Render the scene.
    void RainDrop::OnRender()
    {
        // Let the compute thread know that a new frame is being rendered.
        InterlockedExchange(&m_render_context_fence_value1, m_render_context_fence_value);

        // Compute work must be completed before the frame can render or else the SRV 
        // will be in the wrong state.
        UINT64 thread_fence_value = InterlockedGetValue(&m_thread_fence_value);
        if (m_thread_fence->GetCompletedValue() < thread_fence_value)
        {
            // Instruct the rendering command queue to wait for the current 
            // compute work to complete.
            ThrowIfFailed(m_command.command_queue()->Wait(m_thread_fence.Get(), thread_fence_value));
        }

        //PIXBeginEvent(m_command.command_queue(), 0, L"Render");
        m_command.BeginFrame();

        if (deferred_releases_flag[m_frame_index])
        {
            process_deferred_releases(m_frame_index);
        }

        // Record all the commands we need to render the scene into the command list.
        PopulateCommandList();

        MoveToNextFrame();
    }

    // Fill the command list with all the render commands and dependent state.
    void RainDrop::PopulateCommandList()
    {
        // Set necessary state.
        m_command.command_list()->SetPipelineState(m_pipeline_state.Get());
        m_command.command_list()->SetGraphicsRootSignature(m_root_signature.Get());

        m_command.command_list()->SetGraphicsRootConstantBufferView(Root_Parameter_CB, m_constant_buffer_gs->GetGPUVirtualAddress() + m_frame_index * sizeof(Constant_Buffer_GS));

        ID3D12DescriptorHeap* p_p_heaps[] = { m_srv_uav_heap.Get() };
        m_command.command_list()->SetDescriptorHeaps(_countof(p_p_heaps), p_p_heaps);

        m_command.command_list()->IASetVertexBuffers(0, 1, &m_vertex_buffer_view);
        m_command.command_list()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
        m_command.command_list()->RSSetScissorRects(1, &m_surface.scissor_rect());

        barriers::transition_resource(m_command.command_list(), m_surface.back_buffer(),
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        // Indicate that the back buffer will be used as a render target.
        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle{ m_surface.rtv().ptr };
        D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = m_depth_buffer.dsv();
        m_command.command_list()->OMSetRenderTargets(1, &rtv_handle, FALSE, &dsv_handle);

        // Record commands.
        const float clearColor[] = { 0.0f, 0.0f, 0.1f, 0.0f };
        m_command.command_list()->ClearRenderTargetView(rtv_handle, clearColor, 0, nullptr);
        m_command.command_list()->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        // Render the particles.
        const UINT srv_index = (m_srv_index == 0 ? Srv_Particle_Pos_Vel_0 : Srv_Particle_Pos_Vel_1);

        m_command.command_list()->RSSetViewports(1, &m_surface.viewport());

        D3D12_GPU_DESCRIPTOR_HANDLE srv_handle = m_srv_uav_heap->GetGPUDescriptorHandleForHeapStart();
        srv_handle.ptr += (SIZE_T)(srv_index * m_srv_uav_descriptor_size);
        m_command.command_list()->SetGraphicsRootDescriptorTable(Root_Parameter_SRV, srv_handle);

        PIXBeginEvent(m_command.command_list(), 0, L"Draw particles for thread");
        m_command.command_list()->DrawInstanced(Particle_Count, 1, 0, 0);
        PIXEndEvent(m_command.command_list());

        // Indicate that the back buffer will now be used to present.
        barriers::transition_resource(m_command.command_list(), m_surface.back_buffer(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    }

    void RainDrop::deferred_release(IUnknown* resource)
    {
        const UINT frame_idx{ current_frame_index() };
        std::lock_guard lock{ deferred_releases_mutex };
        deferred_releases[frame_idx].push_back(resource);
        set_deferred_releases_flag();
    }

    void __declspec(noinline) RainDrop::process_deferred_releases(UINT frame_index)
    {
        std::lock_guard lock{ deferred_releases_mutex };
        // NOTE: we clear this flag in the beginning. If we'd clear it at the end
        //       then it might overwrite some other thread that was trying to set it.
        //       It's fine if overwriting happens before processing the items.
        deferred_releases_flag[m_frame_index] = false;

        m_rtv_desc_heap.process_deferred_free(frame_index);
        m_dsv_desc_heap.process_deferred_free(frame_index);
        m_srv_desc_heap.process_deferred_free(frame_index);
        m_uav_desc_heap.process_deferred_free(frame_index);

        std::vector<IUnknown*>& resources{ deferred_releases[m_frame_index] };
        if (!resources.empty())
        {
            for (auto& resource : resources)
            {
                core::release(resource);
            }
            resources.clear();
        }
    }

    DWORD RainDrop::AsyncComputeThreadProc(int thread_index)
    {
        // Get the global resource to local resource
        ID3D12CommandQueue* p_command_queue = m_compute_command_queue.Get();
        ID3D12CommandAllocator* p_command_allocator = m_compute_command_allocator.Get();
        ID3D12GraphicsCommandList* p_command_list = m_compute_command_list.Get();
        ID3D12Fence* p_fence = m_thread_fence.Get();

        while (0 == InterlockedGetValue(&m_terminating))
        {

            // Run the particle simulation.
            Simulate();

            // Close and execute the command list.
            ThrowIfFailed(p_command_list->Close());
            ID3D12CommandList* p_p_command_lists[] = { p_command_list };

            PIXBeginEvent(p_command_queue, 0, L"Thread %d: Iterate on the particle simulation", thread_index);
            p_command_queue->ExecuteCommandLists(1, p_p_command_lists);
            PIXEndEvent(p_command_queue);

            // Wait for the compute shader to complete the simulation.
            UINT64 thread_fence_value = _InterlockedIncrement(&m_thread_fence_value);
            ThrowIfFailed(p_command_queue->Signal(p_fence, thread_fence_value));
            ThrowIfFailed(p_fence->SetEventOnCompletion(thread_fence_value, m_thread_fence_event));
            WaitForSingleObject(m_thread_fence_event, INFINITE);

            // Wait for the render thread to be done with the SRV so that
            // the next frame in the simulation can run.
            UINT64 render_context_fence_value = InterlockedGetValue(&m_render_context_fence_value1);
            if (m_render_context_fence->GetCompletedValue() < render_context_fence_value)
            {
                ThrowIfFailed(p_command_queue->Wait(m_render_context_fence.Get(), render_context_fence_value));
                InterlockedExchange(&m_render_context_fence_value1, 0);
            }

            // Swap the indices to the SRV and UAV.
            m_srv_index = 1 - m_srv_index;

            // Prepare for the next frame.
            ThrowIfFailed(p_command_allocator->Reset());
            ThrowIfFailed(p_command_list->Reset(p_command_allocator, m_compute_state.Get()));
        }

        return 0;
    }

    // Run the particle simulation using the compute shader.
    void RainDrop::Simulate()
    {
        id3d12_graphics_command_list* p_command_list = m_compute_command_list.Get();

        UINT srv_index;
        UINT uav_index;
        ID3D12Resource* p_uav_resource;

        // Flip the buffers
        if (m_srv_index)
        {
            srv_index = Srv_Particle_Pos_Vel_0;
            uav_index = Uav_Particle_Pos_Vel_1;
            p_uav_resource = m_particle_buffer_1.Get();
        }
        else
        {
            srv_index = Srv_Particle_Pos_Vel_1;
            uav_index = Uav_Particle_Pos_Vel_0;
            p_uav_resource = m_particle_buffer_0.Get();
        }

        barriers::transition_resource(p_command_list, p_uav_resource,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        p_command_list->SetPipelineState(m_compute_state.Get());
        p_command_list->SetComputeRootSignature(m_compute_root_signature.Get());

        ID3D12DescriptorHeap* p_p_heap[] = { m_srv_uav_heap.Get() };
        p_command_list->SetDescriptorHeaps(_countof(p_p_heap), p_p_heap);

        D3D12_GPU_DESCRIPTOR_HANDLE srv_handle = m_srv_uav_heap->GetGPUDescriptorHandleForHeapStart();
        srv_handle.ptr += (SIZE_T)(srv_index * m_srv_uav_descriptor_size);
        D3D12_GPU_DESCRIPTOR_HANDLE uav_handle = m_srv_uav_heap->GetGPUDescriptorHandleForHeapStart();
        uav_handle.ptr += (SIZE_T)(uav_index * m_srv_uav_descriptor_size);

        p_command_list->SetComputeRootConstantBufferView(Root_Parameter_CB, m_constant_buffer_cs->GetGPUVirtualAddress());
        p_command_list->SetComputeRootDescriptorTable(Root_Parameter_SRV, srv_handle);
        p_command_list->SetComputeRootDescriptorTable(Root_Parameter_UAV, uav_handle);

        p_command_list->Dispatch(static_cast<int>(ceil(Particle_Count / 128.0f)), 1, 1);

        barriers::transition_resource(p_command_list, p_uav_resource,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }

    void RainDrop::OnDestroy()
    {
        m_command.Release();

        // NOTE: we don't call process_deferred_releases at the end because
        //       some resources (such as swap chains) can't be released before
        //       their depending resources are released.
        for (UINT i{ 0 }; i < Frame_Count; ++i)
        {
            process_deferred_releases(i);
        }

        upload::shutdown();

        m_render_target.release();

        m_depth_buffer.release();

        m_surface.release();

        // NOTE: some modules free their descriptors when they shutdown.
        //       We process those by calling process_deferred_free once more.
        m_rtv_desc_heap.process_deferred_free(0);
        m_dsv_desc_heap.process_deferred_free(0);
        m_srv_desc_heap.process_deferred_free(0);
        m_uav_desc_heap.process_deferred_free(0);

        for (UINT i{ 0 }; i < Frame_Count; ++i)
        {
            process_deferred_releases(i);
        }

        m_rtv_desc_heap.release();
        m_dsv_desc_heap.release();
        m_srv_desc_heap.release();
        m_uav_desc_heap.release();

        for (UINT i{ 0 }; i < Frame_Count; ++i)
        {
            process_deferred_releases(i);
        }

        // Notify the compute threads that the app is shutting down.
        InterlockedExchange(&m_terminating, 1);
        WaitForMultipleObjects(1, &m_thread_handle, TRUE, INFINITE);

        // Ensure that the GPU is no longer referencing resources that are about to be
        // cleaned up by the destructor.
        WaitForRenderContext();

        // Close handles to fence events and threads.
        CloseHandle(m_render_context_fence_event);
        CloseHandle(m_thread_handle);
        CloseHandle(m_thread_fence_event);
    }

    void RainDrop::OnKeyDown(UINT8 key)
    {
        m_camera.OnKeyDown(key);
    }

    void RainDrop::OnKeyUp(UINT8 key)
    {
        m_camera.OnKeyUp(key);
    }

    void RainDrop::create_surface(HWND hwnd, UINT width, UINT height)
    {
        new (&m_surface) Surface(hwnd, width, height);
        m_surface.create_swap_chain(m_factory.Get(), m_command.command_queue());
    }

    void RainDrop::remove_surface()
    {
        m_command.Flush();
    }

    void RainDrop::WaitForRenderContext()
    {
        // Add a signal command to the queue.
        ThrowIfFailed(m_command.command_queue()->Signal(m_render_context_fence.Get(), m_render_context_fence_value));

        // Instruct the fence to set the event object when the signal command completes.
        ThrowIfFailed(m_render_context_fence->SetEventOnCompletion(m_render_context_fence_value, m_render_context_fence_event));

        ++m_render_context_fence_value;

        // Wait until the signal command has been processed.
        WaitForSingleObject(m_render_context_fence_event, INFINITE);
    }

    // Cycle through the frame resources. This method blocks execution if the 
    // next frame resource in the queue has not yet had its previous contents 
    // processed by the GPU.
    void RainDrop::MoveToNextFrame()
    {
        m_command.EndFrame(m_surface);

        // Assign the current fence value to the current frame.
        m_frame_fence_values[m_frame_index] = m_render_context_fence_value;

        // Signal and increment the fence value.
        ThrowIfFailed(m_command.command_queue()->Signal(m_render_context_fence.Get(), m_render_context_fence_value));
        m_render_context_fence_value++;

        // Update the frame index.
        m_frame_index = m_surface.set_current_bb_index();

        // If the next frame is not ready to be rendered yet, wait until it is ready.
        if (m_render_context_fence->GetCompletedValue() < m_frame_fence_values[m_frame_index])
        {
            ThrowIfFailed(m_render_context_fence->SetEventOnCompletion(m_frame_fence_values[m_frame_index], m_render_context_fence_event));
            WaitForSingleObject(m_render_context_fence_event, INFINITE);
        }
    }
}