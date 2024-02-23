#include "stdafx.h"
#include "D3D12RainDrop.h"

// InterlockedCompareExchange returns the object's value if the 
// comparison fails.  If it is already 0, then its value won't 
// change and 0 will be returned.
#define InterlockedGetValue(object) InterlockedCompareExchange(object, 0, 0)

const float D3D12RainDrop::Particle_Spread = 400.0f;

D3D12RainDrop::D3D12RainDrop(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    m_frame_index(0),
    m_viewport(),
    m_scissor_rect(),
    m_rtv_descriptor_size(0),
    m_srv_uav_descriptor_size(0),
    m_pConstantBufferGSData(nullptr),
    m_render_context_fence_value(0),
    m_terminating(0)
{
    srand(0);

    ZeroMemory(m_srv_index, sizeof(m_srv_index));
    ZeroMemory(m_frame_fence_values, sizeof(m_frame_fence_values));

    for (int n = 0; n < Thread_Count; n++)
    {
        m_render_context_fence_values[n] = 0;
        m_thread_fence_values[n] = 0;
    }

    m_viewport.Width = static_cast<float>(width);
    m_viewport.Height = static_cast<float>(height);
    m_viewport.MaxDepth = 1.0f;

    m_scissor_rect.right = static_cast<LONG>(width);
    m_scissor_rect.bottom = static_cast<LONG>(height);

    float sqRootNumAsyncContexts = (float)sqrt(static_cast<float>(Thread_Count));
    m_height_instances = static_cast<UINT>(ceil(sqRootNumAsyncContexts));
    m_width_instances = static_cast<UINT>(ceil(sqRootNumAsyncContexts));

    if (m_width_instances * (m_height_instances - 1) >= Thread_Count)
    {
        --m_height_instances;
    }
}

void D3D12RainDrop::OnInit()
{
    m_camera.Init({ 0.0f, 0.0f, 1500.0f });
    m_camera.SetMoveSpeed(250.0f);

    LoadPipeline();
    LoadAssets();
    CreateAsyncContexts();
}

// Load the rendering pipeline dependencies.
void D3D12RainDrop::LoadPipeline()
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

ComPtr<IDXGIFactory4> factory;
ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

if (m_use_warp_device)
{
    // Wrap device
    ComPtr<IDXGIAdapter> warp_adapter;
    ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter)));
    ThrowIfFailed(D3D12CreateDevice(warp_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
}
else
{
    ComPtr<IDXGIAdapter1> hardware_adapter;
    GetHardwareAdapter(factory.Get(), &hardware_adapter);

    ThrowIfFailed(D3D12CreateDevice(hardware_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
}

// Command Queue
D3D12_COMMAND_QUEUE_DESC queue_desc{};
queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;          // D3D12_COMMAND_LIST_TYPE Type;
queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL; // INT Priority;
queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;          // D3D12_COMMAND_QUEUE_FLAGS Flags;
queue_desc.NodeMask = 0;                                   // UINT NodeMask;

ThrowIfFailed(m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_command_queue)));
NAME_D3D12_OBJECT(m_command_queue);

// Swap Chain
DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
swap_chain_desc.BufferCount = Frame_Count;                                         // UINT BufferCount;
swap_chain_desc.Width = m_width;                                                  // UINT Width;
swap_chain_desc.Height = m_height;                                                // UINT Height;
swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;                              // DXGI_FORMAT Format;
swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;                    // DXGI_USAGE BufferUsage;
swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;                       // DXGI_SWAP_EFFECT SwapEffect;
swap_chain_desc.SampleDesc = { 1, 0 };                                            // DXGI_SAMPLE_DESC SampleDesc;
swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;       // UINT Flags;

swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;                                   // DXGI_SCALING Scaling;
swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;                          // DXGI_ALPHA_MODE AlphaMode;
swap_chain_desc.Stereo = 0;                                                       // BOOL Stereo;

ComPtr<IDXGISwapChain1> swap_chain;
// Swap chain needs the queue so that it can force a flush on it.
ThrowIfFailed(factory->CreateSwapChainForHwnd(m_command_queue.Get(), Win32Application::GetHwnd(), &swap_chain_desc, nullptr, nullptr, &swap_chain));

// This sample does not support full screen transitions.
ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

ThrowIfFailed(swap_chain.As(&m_swap_chain));
m_frame_index = m_swap_chain->GetCurrentBackBufferIndex();
m_swap_chain_event = m_swap_chain->GetFrameLatencyWaitableObject();

// descriptor heap
{
    // render target view (RTV) descriptor heap.
    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
    rtv_heap_desc.NumDescriptors = Frame_Count;                  // UINT NumDescriptors;
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;      // D3D12_DESCRIPTOR_HEAP_TYPE Type;
    rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;    // D3D12_DESCRIPTOR_HEAP_FLAGS Flags;

    rtv_heap_desc.NodeMask = 0;                        // UINT NodeMask;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&m_rtv_heap)));
    NAME_D3D12_OBJECT(m_rtv_heap);

    m_rtv_descriptor_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Describe and create a shader resource view (SRV) and unordered
    // access view (UAV) descriptor heap.
    D3D12_DESCRIPTOR_HEAP_DESC srv_uav_heap_desc = {};
    srv_uav_heap_desc.NumDescriptors = Descriptor_Count;                  // UINT NumDescriptors;
    srv_uav_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;      // D3D12_DESCRIPTOR_HEAP_TYPE Type;
    srv_uav_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;    // D3D12_DESCRIPTOR_HEAP_FLAGS Flags;
    srv_uav_heap_desc.NodeMask = 0;                        // UINT NodeMask;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&srv_uav_heap_desc, IID_PPV_ARGS(&m_srv_uav_heap)));
    NAME_D3D12_OBJECT(m_srv_uav_heap);

    m_srv_uav_descriptor_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

// Create frame resources.
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle{ m_rtv_heap->GetCPUDescriptorHandleForHeapStart() };

    // Create a RTV and a command allocator for each frame
    for (UINT n = 0; n < Frame_Count; ++n)
    {
        ThrowIfFailed(m_swap_chain->GetBuffer(n, IID_PPV_ARGS(&m_render_targets[n])));
        m_device->CreateRenderTargetView(m_render_targets[n].Get(), nullptr, rtv_handle);
        NAME_D3D12_OBJECT_INDEXED(m_render_targets, n);

        rtv_handle.ptr += (SIZE_T)m_rtv_descriptor_size;

        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_command_allocators[n])));
    }
}
}

void D3D12RainDrop::LoadAssets()
{
    // Create the root signatures.
    {
        D3D12_DESCRIPTOR_RANGE ranges[2];
        ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // D3D12_DESCRIPTOR_RANGE_TYPE RangeType;
        ranges[0].NumDescriptors = 1;                          // UINT NumDescriptors;
        ranges[0].BaseShaderRegister = 0;                      // UINT BaseShaderRegister;
        ranges[0].RegisterSpace = 0;                           // UINT RegisterSpace;
        ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;       // UINT OffsetInDescriptorsFromTableStart;

        ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; // D3D12_DESCRIPTOR_RANGE_TYPE RangeType;
        ranges[1].NumDescriptors = 1;                          // UINT NumDescriptors;
        ranges[1].BaseShaderRegister = 0;                      // UINT BaseShaderRegister;
        ranges[1].RegisterSpace = 0;                           // UINT RegisterSpace;
        ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;       // UINT OffsetInDescriptorsFromTableStart;

        D3D12_ROOT_PARAMETER root_parameters[Root_Parameters_Count];
        // Constant Buffer
        root_parameters[Root_Parameter_CB].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        root_parameters[Root_Parameter_CB].Constants.ShaderRegister = 0;
        root_parameters[Root_Parameter_CB].Constants.RegisterSpace = 0;
        root_parameters[Root_Parameter_CB].Constants.Num32BitValues = 0;
        root_parameters[Root_Parameter_CB].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        // SRV Descriptor Table
        root_parameters[Root_Parameter_SRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_parameters[Root_Parameter_SRV].DescriptorTable.NumDescriptorRanges = 1;
        root_parameters[Root_Parameter_SRV].DescriptorTable.pDescriptorRanges = &ranges[0];
        root_parameters[Root_Parameter_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        // UAV Descriptor Table
        root_parameters[Root_Parameter_UAV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_parameters[Root_Parameter_UAV].DescriptorTable.NumDescriptorRanges = 1;
        root_parameters[Root_Parameter_UAV].DescriptorTable.pDescriptorRanges = &ranges[1];
        root_parameters[Root_Parameter_UAV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        // The rendering pipeline does not need the UAV parameter.
        D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
        root_signature_desc.NumParameters = Root_Parameter_SRV + 1;                               // UINT NumParameters;
        root_signature_desc.pParameters = root_parameters;                                        // _Field_size_full_(NumParameters)  const D3D12_ROOT_PARAMETER* pParameters;
        root_signature_desc.NumStaticSamplers = 0;                                                // UINT NumStaticSamplers;
        root_signature_desc.pStaticSamplers = nullptr;                                            // _Field_size_full_(NumStaticSamplers)  const D3D12_STATIC_SAMPLER_DESC* pStaticSamplers;
        root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT; // D3D12_ROOT_SIGNATURE_FLAGS Flags;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;

        ThrowIfFailed(D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_root_signature)));
        NAME_D3D12_OBJECT(m_root_signature);

        // Create compute root signature
        root_parameters[Root_Parameter_SRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC compute_root_signature_desc = {};
        compute_root_signature_desc.NumParameters = Root_Parameters_Count;                         // UINT NumParameters;
        compute_root_signature_desc.pParameters = root_parameters;                                // _Field_size_full_(NumParameters)  const D3D12_ROOT_PARAMETER* pParameters;
        compute_root_signature_desc.NumStaticSamplers = 0;                                        // UINT NumStaticSamplers;
        compute_root_signature_desc.pStaticSamplers = nullptr;                                    // _Field_size_full_(NumStaticSamplers)  const D3D12_STATIC_SAMPLER_DESC* pStaticSamplers;
        compute_root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;                       // D3D12_ROOT_SIGNATURE_FLAGS Flags;
        ThrowIfFailed(D3D12SerializeRootSignature(&compute_root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));

        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_compute_root_signature)));
        NAME_D3D12_OBJECT(m_compute_root_signature);
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

        // Load and compile shaders.
        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"ParticleDraw.hlsl").c_str(), nullptr, nullptr, "VSParticleDraw", "vs_5_0", compile_flags, 0, &vertex_shader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"ParticleDraw.hlsl").c_str(), nullptr, nullptr, "GSParticleDraw", "gs_5_0", compile_flags, 0, &geometry_shader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"ParticleDraw.hlsl").c_str(), nullptr, nullptr, "PSParticleDraw", "ps_5_0", compile_flags, 0, &pixel_shader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"NBodyGravityCS.hlsl").c_str(), nullptr, nullptr, "CSMain", "cs_5_0", compile_flags, 0, &compute_shader, nullptr));

        // D3D12_INPUT_ELEMENT_DESC
        //    LPCSTR SemanticName;
        //    UINT SemanticIndex;
        //    DXGI_FORMAT Format;
        //    UINT InputSlot;
        //    UINT AlignedByteOffset;
        //    D3D12_INPUT_CLASSIFICATION InputSlotClass;
        //    UINT InstanceDataStepRate;
        D3D12_INPUT_ELEMENT_DESC input_element_descs[] =
        {
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
        pso_desc.pRootSignature = m_root_signature.Get();                                        // ID3D12RootSignature* pRootSignature;
        pso_desc.VS = { vertex_shader->GetBufferPointer(), vertex_shader->GetBufferSize() };     // D3D12_SHADER_BYTECODE VS;
        pso_desc.PS = { pixel_shader->GetBufferPointer(), pixel_shader->GetBufferSize() };       // D3D12_SHADER_BYTECODE PS;
        pso_desc.DS = {};                                                                        // D3D12_SHADER_BYTECODE DS;
        pso_desc.HS = {};                                                                        // D3D12_SHADER_BYTECODE HS;
        pso_desc.GS = { geometry_shader->GetBufferPointer(), geometry_shader->GetBufferSize() }; // D3D12_SHADER_BYTECODE GS;
        pso_desc.StreamOutput = {};                                                              // D3D12_STREAM_OUTPUT_DESC StreamOutput;
        pso_desc.BlendState = blend_state.blend_desc;                                            // D3D12_BLEND_DESC BlendState;
        pso_desc.SampleMask = UINT_MAX;                                                          // UINT SampleMask;
        pso_desc.RasterizerState = rasterizer_state.face_cull;                                   // D3D12_RASTERIZER_DESC RasterizerState;
        pso_desc.DepthStencilState = depth_desc_state.disable;                                   // D3D12_DEPTH_STENCIL_DESC DepthStencilState;
        pso_desc.InputLayout = { input_element_descs, _countof(input_element_descs) };           // D3D12_INPUT_LAYOUT_DESC InputLayout;
        pso_desc.IBStripCutValue = {};                                                           // D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
        pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;                    // D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType;
        pso_desc.NumRenderTargets = 1;                                                           // UINT NumRenderTargets;
        pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;                                     // DXGI_FORMAT RTVFormats[8];
        pso_desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;                                      // DXGI_FORMAT DSVFormat;
        pso_desc.SampleDesc = { 1, 0 };                                                          // DXGI_SAMPLE_DESC SampleDesc;
        pso_desc.NodeMask = 0;                                                                   // UINT NodeMask;
        pso_desc.CachedPSO = {};                                                                 // D3D12_CACHED_PIPELINE_STATE CachedPSO;
        pso_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;                                         // D3D12_PIPELINE_STATE_FLAGS Flags;

        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&m_pipeline_state)));
        NAME_D3D12_OBJECT(m_pipeline_state);

        // Describe and create the compute pipeline state object (PSO).
        D3D12_COMPUTE_PIPELINE_STATE_DESC compute_pso_desc = {};
        compute_pso_desc.pRootSignature = m_compute_root_signature.Get();       //   ID3D12RootSignature* pRootSignature;
        compute_pso_desc.CS = { compute_shader->GetBufferPointer(), compute_shader->GetBufferSize() }; // D3D12_SHADER_BYTECODE CS;
        compute_pso_desc.NodeMask = 0;                                          //   UINT NodeMask;
        compute_pso_desc.CachedPSO = {};                                        //   D3D12_CACHED_PIPELINE_STATE CachedPSO;
        compute_pso_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;                //   D3D12_PIPELINE_STATE_FLAGS Flags;

        ThrowIfFailed(m_device->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&m_compute_state)));
        NAME_D3D12_OBJECT(m_compute_state);
    }

    // Create the command list.
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_command_allocators[m_frame_index].Get(), m_pipeline_state.Get(), IID_PPV_ARGS(&m_command_list)));
    NAME_D3D12_OBJECT(m_command_list);

    CreateVertexBuffer();
    CreateParticleBuffers();

    // Note: ComPtr's are CPU objects but this resource needs to stay in scope until
    // the command list that references it has finished executing on the GPU.
    // We will flush the GPU at the end of this method to ensure the resource is not
    // prematurely destroyed.
    // Other words-  Complete the GPU work before ending the routine and auto removing the constantBufferCSUpload.
    ComPtr<ID3D12Resource> constant_buffer_cs_upload;

    // Create the compute shader's constant buffer.
    {
        const UINT buffer_size = sizeof(Constant_Buffer_CS);

        D3D12_RESOURCE_DESC buffer_desc = {};
        buffer_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;        // D3D12_RESOURCE_DIMENSION Dimension;
        buffer_desc.Alignment = 0;                                      // UINT64 Alignment;
        buffer_desc.Width = buffer_size;                                  // UINT64 Width;
        buffer_desc.Height = 1;                                         // UINT Height;
        buffer_desc.DepthOrArraySize = 1;                               // UINT16 DepthOrArraySize;
        buffer_desc.MipLevels = 1;                                      // UINT16 MipLevels;
        buffer_desc.Format = DXGI_FORMAT_UNKNOWN;                       // DXGI_FORMAT Format;
        buffer_desc.SampleDesc = { 1, 0 };                              // DXGI_SAMPLE_DESC SampleDesc;
        buffer_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;              // D3D12_TEXTURE_LAYOUT Layout;
        buffer_desc.Flags = D3D12_RESOURCE_FLAG_NONE; // D3D12_RESOURCE_FLAGS Flags;

        ThrowIfFailed(m_device->CreateCommittedResource(&heap_properties.default_heap, D3D12_HEAP_FLAG_NONE, &buffer_desc,
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_constant_buffer_cs)));
        NAME_D3D12_OBJECT(m_constant_buffer_cs);

        //buffer_desc.Flags = D3D12_RESOURCE_FLAG_NONE; // D3D12_RESOURCE_FLAGS Flags;
        ThrowIfFailed(m_device->CreateCommittedResource(&heap_properties.upload_heap, D3D12_HEAP_FLAG_NONE, &buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&constant_buffer_cs_upload)));
        NAME_D3D12_OBJECT(constant_buffer_cs_upload);

        Constant_Buffer_CS constant_buffer_cs = {};
        constant_buffer_cs.param[0] = Particle_Count;
        constant_buffer_cs.param[1] = int(ceil(Particle_Count / 128.0f));
        constant_buffer_cs.paramf[0] = 0.1f;
        constant_buffer_cs.paramf[1] = 1.0f;

        D3D12_SUBRESOURCE_DATA compute_cb_data = {};
        compute_cb_data.pData = reinterpret_cast<UINT8*>(&constant_buffer_cs);
        compute_cb_data.RowPitch = buffer_size;
        compute_cb_data.SlicePitch = buffer_size;

        Update_Subresource(m_constant_buffer_cs.Get(), constant_buffer_cs_upload.Get(), &compute_cb_data);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = m_constant_buffer_cs.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        m_command_list->ResourceBarrier(1, &barrier);
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

        ThrowIfFailed(m_device->CreateCommittedResource(&heap_properties.upload_heap, D3D12_HEAP_FLAG_NONE, &buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_constant_buffer_gs)));
        NAME_D3D12_OBJECT(m_constant_buffer_gs);

        D3D12_RANGE read_range = {}; // We do not intend to read from this resource on the CPU.
        read_range.Begin = 0;
        read_range.End = 0;
        ThrowIfFailed(m_constant_buffer_gs->Map(0, &read_range, reinterpret_cast<void**>(&m_p_constant_buffer_gs_data)));
        ZeroMemory(m_p_constant_buffer_gs_data, constant_buffer_gs_size);
    }

    // Close the command list and execute it to begin the initial GPU setup and uploads.
    ThrowIfFailed(m_command_list->Close());
    ID3D12CommandList* pp_command_lists[] = { m_command_list.Get() };
    m_command_queue->ExecuteCommandLists(_countof(pp_command_lists), pp_command_lists);

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

// IntermediateOffset = 0;
// FirstSubresource = 0;
// NumSubresources = 1;

UINT64 D3D12RainDrop::Update_Subresource(ID3D12Resource* p_destination_resource, ID3D12Resource* p_intermediate_resource, D3D12_SUBRESOURCE_DATA* p_src_data)
{
    // Call 1
    const UINT Number_Subresources = 1;
    UINT64 required_size = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts;
    UINT number_of_rows;
    UINT64 row_sizes_in_bytes;

    D3D12_RESOURCE_DESC destination_desc = p_destination_resource->GetDesc();
    m_device->GetCopyableFootprints(&destination_desc, 0, 1, 0, &layouts, &number_of_rows, &row_sizes_in_bytes, &required_size);
    D3D12_RESOURCE_DESC intermediate_desc = p_intermediate_resource->GetDesc();
    m_device->GetCopyableFootprints(&intermediate_desc, 0, 1, 0, &layouts, &number_of_rows, &row_sizes_in_bytes, &required_size);

    // 	return UpdateSubresources(pCmdList, pDestinationResource, pIntermediate, FirstSubresource, NumSubresources, RequiredSize, Layouts, NumRows, RowSizesInBytes, pSrcData);
    //  Call 2                                                                          0                 1                
    UINT64 memory_to_allocate = static_cast<UINT64>(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) + sizeof(UINT) + sizeof(UINT64)) * Number_Subresources;
    if (memory_to_allocate > SIZE_MAX)
    {
        throw;
    }

    void* p_memory = HeapAlloc(GetProcessHeap(), 0, static_cast<SIZE_T>(memory_to_allocate));
    if (p_memory == NULL)
    {
        throw;
    }

    if (intermediate_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER ||
        intermediate_desc.Width < required_size + layouts.Offset ||
        required_size  >(SIZE_T) - 1 || row_sizes_in_bytes > (SIZE_T)-1 ||
        destination_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
    {
        throw;
    }

    BYTE* p_intermediate_data;
    ThrowIfFailed(p_intermediate_resource->Map(0, NULL, reinterpret_cast<void**>(&p_intermediate_data)));

    D3D12_MEMCPY_DEST dest_data = { p_intermediate_data + layouts.Offset, layouts.Footprint.RowPitch, layouts.Footprint.RowPitch * number_of_rows };
    // MemcpySubresource(&DestData, &pSrcData[i], (SIZE_T)pRowSizesInBytes[i], pNumRows[i], pLayouts[i].Footprint.Depth);
    // MemcpySubresource(&dest_data, &p_src_data[0], row_sizes_in_bytes, number_of_rows, layouts.Footprint.Depth);
    for (UINT z = 0; z < layouts.Footprint.Depth; ++z)
    {
        BYTE* p_desc_slice = reinterpret_cast<BYTE*>(dest_data.pData) + dest_data.SlicePitch * z;
        const BYTE* p_src_slice = reinterpret_cast<const BYTE*>(p_src_data->pData) + p_src_data->SlicePitch * z;
        for (UINT y = 0; y < number_of_rows; ++y)
        {
            memcpy(p_desc_slice + dest_data.RowPitch * y, p_src_slice + p_src_data->RowPitch * y, row_sizes_in_bytes);
        }
    }
    // MemcpySubresource -------------------

    p_intermediate_resource->Unmap(0, NULL);

    if (destination_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
    {
        throw;
    }

    //D3D12_BOX src_box = {};
    //src_box.left = (UINT)layouts.Offset;
    //src_box.top = 0;
    //src_box.front = 0;
    //src_box.right = (UINT)(layouts.Offset + layouts.Footprint.Width);
    //src_box.bottom = 1;
    //src_box.back = 1;

    m_command_list->CopyBufferRegion(p_destination_resource, 0, p_intermediate_resource, (UINT)layouts.Offset, layouts.Footprint.Width);

    return required_size;
}

// Create the particle vertex buffer.
void D3D12RainDrop::CreateVertexBuffer()
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

    D3D12_RESOURCE_DESC resource_desc = {};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;                 //     D3D12_RESOURCE_DIMENSION Dimension; 
    resource_desc.Alignment = 0;                                               //     UINT64 Alignment;
    resource_desc.Width = buffer_size;                                         //     UINT64 Width;
    resource_desc.Height = 1;                                                  //     UINT Height;
    resource_desc.DepthOrArraySize = 1;                                        //     UINT16 DepthOrArraySize;
    resource_desc.MipLevels = 1;                                               //     UINT16 MipLevels;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;                                //     DXGI_FORMAT Format;
    resource_desc.SampleDesc = { 1, 0 };                                       //     DXGI_SAMPLE_DESC SampleDesc;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;                     //     D3D12_TEXTURE_LAYOUT Layout;
    resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;                            //     D3D12_RESOURCE_FLAGS Flags;

    ThrowIfFailed(m_device->CreateCommittedResource(&heap_properties.default_heap, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_vertex_buffer)));
    NAME_D3D12_OBJECT(m_vertex_buffer);

    ThrowIfFailed(m_device->CreateCommittedResource(&heap_properties.upload_heap, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_vertex_buffer_upload)));
    NAME_D3D12_OBJECT(m_vertex_buffer_upload);

    // Upload the data
    D3D12_SUBRESOURCE_DATA vertex_data = {};
    vertex_data.pData = reinterpret_cast<UINT8*>(&vertices[0]);                // const void* pData;
    vertex_data.RowPitch = buffer_size;                                        // LONG_PTR RowPitch;
    vertex_data.SlicePitch = buffer_size;                                      // LONG_PTR SlicePitch;

    Update_Subresource(m_vertex_buffer.Get(), m_vertex_buffer_upload.Get(), &vertex_data);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_vertex_buffer.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    m_command_list->ResourceBarrier(1, &barrier);

    m_vertex_buffer_view.BufferLocation = m_vertex_buffer->GetGPUVirtualAddress();
    m_vertex_buffer_view.SizeInBytes = static_cast<UINT>(buffer_size);
    m_vertex_buffer_view.StrideInBytes = sizeof(Particle_Vertex);
}

// Random percent value, from -1 to 1.
float D3D12RainDrop::RandomPercent()
{
    float ret = static_cast<float>((rand() % 10000) - 5000);
    return ret / 5000.0f;
}

void D3D12RainDrop::LoadParticles(_Out_writes_(number_of_paricles) Particle* p_particles, const XMFLOAT3& center, const XMFLOAT4& velocity, float spread, UINT number_of_paricles)
{
    for (UINT i = 0; i < number_of_paricles; ++i)
    {
        XMFLOAT3 delta(spread, spread, spread);

        // Find min length
        while (XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&delta))) > spread * spread)
        {
            delta.x = RandomPercent() * spread;
            delta.y = RandomPercent() * spread;
            delta.z = RandomPercent() * spread;
        }

        p_particles[i].position.x = center.x + delta.x;
        p_particles[i].position.y = center.y + delta.y;
        p_particles[i].position.z = center.z + delta.z;
        p_particles[i].position.w = 10000.0f * 10000.0f;

        p_particles[i].velocity = velocity;
    }
}

// Create the position and velocity buffer shader resources.
void D3D12RainDrop::CreateParticleBuffers()
{
    // Initialize the data in the buffers
    std::vector<Particle> data;
    data.resize(Particle_Count);
    const UINT data_size = Particle_Count * sizeof(Particle);

    // Split the particles into two groups.
    float center_spread = Particle_Spread * 0.5f;
    LoadParticles(&data[0], XMFLOAT3(center_spread, 0, 0), XMFLOAT4(0, 0, -20, 1 / 100000000.0f), Particle_Spread, Particle_Count / 2);
    LoadParticles(&data[Particle_Count / 2], XMFLOAT3(-center_spread, 0, 0), XMFLOAT4(0, 0, 20, 1 / 100000000.0f), Particle_Spread, Particle_Count / 2);

    // Get the D3D12_HEAP_PROPERTIES defaultHeapProperties and uploadHeapProperties from helper
    D3D12_RESOURCE_DESC buffer_desc = {};
    buffer_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;        // D3D12_RESOURCE_DIMENSION Dimension;
    buffer_desc.Alignment = 0;                                      // UINT64 Alignment;
    buffer_desc.Width = data_size;                                  // UINT64 Width;
    buffer_desc.Height = 1;                                         // UINT Height;
    buffer_desc.DepthOrArraySize = 1;                               // UINT16 DepthOrArraySize;
    buffer_desc.MipLevels = 1;                                      // UINT16 MipLevels;
    buffer_desc.Format = DXGI_FORMAT_UNKNOWN;                       // DXGI_FORMAT Format;
    buffer_desc.SampleDesc = { 1, 0 };                              // DXGI_SAMPLE_DESC SampleDesc;
    buffer_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;              // D3D12_TEXTURE_LAYOUT Layout;
    buffer_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // D3D12_RESOURCE_FLAGS Flags;

    D3D12_RESOURCE_DESC upload_buffer_desc = {};
    upload_buffer_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;        // D3D12_RESOURCE_DIMENSION Dimension;
    upload_buffer_desc.Alignment = 0;                                      // UINT64 Alignment;
    upload_buffer_desc.Width = data_size;                                  // UINT64 Width;
    upload_buffer_desc.Height = 1;                                         // UINT Height;
    upload_buffer_desc.DepthOrArraySize = 1;                               // UINT16 DepthOrArraySize;
    upload_buffer_desc.MipLevels = 1;                                      // UINT16 MipLevels;
    upload_buffer_desc.Format = DXGI_FORMAT_UNKNOWN;                       // DXGI_FORMAT Format;
    upload_buffer_desc.SampleDesc = { 1, 0 };                              // DXGI_SAMPLE_DESC SampleDesc;
    upload_buffer_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;              // D3D12_TEXTURE_LAYOUT Layout;
    upload_buffer_desc.Flags = D3D12_RESOURCE_FLAG_NONE;                   // D3D12_RESOURCE_FLAGS Flags;

    for (UINT index = 0; index < Thread_Count; ++index)
    {
        // Create two buffers in the GPU, each with a copy of the particles data.
        // The compute shader will update one of them while the rendering thread 
        // renders the other. When rendering completes, the threads will swap 
        // which buffer they work on.

        ThrowIfFailed(m_device->CreateCommittedResource(&heap_properties.default_heap, D3D12_HEAP_FLAG_NONE, &buffer_desc,
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_particle_buffer_0[index])));
        ThrowIfFailed(m_device->CreateCommittedResource(&heap_properties.default_heap, D3D12_HEAP_FLAG_NONE, &buffer_desc,
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_particle_buffer_1[index])));
        NAME_D3D12_OBJECT_INDEXED(m_particle_buffer_0, index);
        NAME_D3D12_OBJECT_INDEXED(m_particle_buffer_1, index);

        ThrowIfFailed(m_device->CreateCommittedResource(&heap_properties.upload_heap, D3D12_HEAP_FLAG_NONE, &upload_buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_particle_buffer_upload_0[index])));
        ThrowIfFailed(m_device->CreateCommittedResource(&heap_properties.upload_heap, D3D12_HEAP_FLAG_NONE, &upload_buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_particle_buffer_upload_1[index])));
        NAME_D3D12_OBJECT_INDEXED(m_particle_buffer_upload_0, index);
        NAME_D3D12_OBJECT_INDEXED(m_particle_buffer_upload_1, index);

        D3D12_SUBRESOURCE_DATA particle_data = {};
        particle_data.pData = reinterpret_cast<UINT8*>(&data[0]);
        particle_data.RowPitch = data_size;
        particle_data.SlicePitch = data_size;

        Update_Subresource(m_particle_buffer_0[index].Get(), m_particle_buffer_upload_0[index].Get(), &particle_data);
        Update_Subresource(m_particle_buffer_1[index].Get(), m_particle_buffer_upload_1[index].Get(), &particle_data);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = m_particle_buffer_0[index].Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

        m_command_list->ResourceBarrier(1, &barrier);

        barrier.Transition.pResource = m_particle_buffer_1[index].Get();

        m_command_list->ResourceBarrier(1, &barrier);

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
        srv_handle_0.ptr += (SIZE_T)((Srv_Particle_Pos_Vel_0 + index) * m_srv_uav_descriptor_size);
        D3D12_CPU_DESCRIPTOR_HANDLE srv_handle_1 = m_srv_uav_heap->GetCPUDescriptorHandleForHeapStart();
        srv_handle_1.ptr += (SIZE_T)((Srv_Particle_Pos_Vel_1 + index) * m_srv_uav_descriptor_size);

        m_device->CreateShaderResourceView(m_particle_buffer_0[index].Get(), &srv_desc, srv_handle_0);
        m_device->CreateShaderResourceView(m_particle_buffer_1[index].Get(), &srv_desc, srv_handle_1);

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
        uav_handle_0.ptr += (Uav_Particle_Pos_Vel_0 + index) * m_srv_uav_descriptor_size;
        D3D12_CPU_DESCRIPTOR_HANDLE uav_handle_1 = m_srv_uav_heap->GetCPUDescriptorHandleForHeapStart();
        uav_handle_1.ptr += (Uav_Particle_Pos_Vel_1 + index) * m_srv_uav_descriptor_size;

        m_device->CreateUnorderedAccessView(m_particle_buffer_0[index].Get(), nullptr, &uav_desc, uav_handle_0);
        m_device->CreateUnorderedAccessView(m_particle_buffer_1[index].Get(), nullptr, &uav_desc, uav_handle_1);
    }
}

void D3D12RainDrop::CreateAsyncContexts()
{
    for (UINT thread_index = 0; thread_index < Thread_Count; ++thread_index)
    {
        // Create the compute resources.
        // D3D12_COMMAND_LIST_TYPE Type;
        // INT Priority;
        // D3D12_COMMAND_QUEUE_FLAGS Flags;
        // UINT NodeMask;
        D3D12_COMMAND_QUEUE_DESC queue_desc = { D3D12_COMMAND_LIST_TYPE_COMPUTE, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0 };
        ThrowIfFailed(m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_compute_command_queue[thread_index])));
        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_compute_command_allocator[thread_index])));
        ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_compute_command_allocator[thread_index].Get(), nullptr, IID_PPV_ARGS(&m_compute_command_list[thread_index])));
        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_thread_fences[thread_index])));

        m_thread_fence_events[thread_index] = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_thread_fence_events[thread_index] == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        m_thread_data[thread_index].p_context = this;
        m_thread_data[thread_index].thread_index = thread_index;

        m_thread_handles[thread_index] = CreateThread(nullptr, 0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(ThreadProc),
            reinterpret_cast<void*>(&m_thread_data[thread_index]),
            CREATE_SUSPENDED, nullptr);

        ResumeThread(m_thread_handles[thread_index]);
    }
}

// Update frame-based values.
void D3D12RainDrop::OnUpdate()
{
    // Wait for the previous Present to complete.
    WaitForSingleObjectEx(m_swap_chain_event, 100, FALSE);

    m_timer.Tick(NULL);
    m_camera.Update(static_cast<float>(m_timer.GetElapsedSeconds()));

    Constant_Buffer_GS constant_buffer_gs = {};
    XMStoreFloat4x4(&constant_buffer_gs.world_view_projection, XMMatrixMultiply(m_camera.GetViewMatrix(), m_camera.GetProjectionMatrix(0.8f, m_aspect_ratio, 1.0f, 5000.0f)));
    XMStoreFloat4x4(&constant_buffer_gs.inverse_view, XMMatrixInverse(nullptr, m_camera.GetViewMatrix()));

    UINT8* destination = m_p_constant_buffer_gs_data + sizeof(Constant_Buffer_GS) * m_frame_index;
    memcpy(destination, &constant_buffer_gs, sizeof(Constant_Buffer_GS));
}

// Render the scene.
void D3D12RainDrop::OnRender()
{
    // Let the compute thread know that a new frame is being rendered.
    for (int n = 0; n < Thread_Count; ++n)
    {
        InterlockedExchange(&m_render_context_fence_values[n], m_render_context_fence_value);
    }

    // Compute work must be completed before the frame can render or else the SRV 
    // will be in the wrong state.
    for (int n = 0; n < Thread_Count; ++n)
    {
        UINT64 thread_fence_value = InterlockedGetValue(&m_thread_fence_values[n]);
        if (m_thread_fences[n]->GetCompletedValue() < thread_fence_value)
        {
            // Instruct the rendering command queue to wait for the current 
            // compute work to complete.
            ThrowIfFailed(m_command_queue->Wait(m_thread_fences[n].Get(), thread_fence_value));
        }
    }

    PIXBeginEvent(m_command_queue.Get(), 0, L"Render");

    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_command_list.Get() };
    m_command_queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    PIXEndEvent(m_command_queue.Get());

    // Present the frame.
    ThrowIfFailed(m_swap_chain->Present(1, 0));

    MoveToNextFrame();
}

// Fill the command list with all the render commands and dependent state.
void D3D12RainDrop::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated
    // command lists have finished execution on the GPU; apps should use
    // fences to determine GPU execution progress.
    ThrowIfFailed(m_command_allocators[m_frame_index]->Reset());

    // However, when ExecuteCommandList() is called on a particular command
    // list, that command list can then be reset at any time and must be before
    // re-recording.
    ThrowIfFailed(m_command_list->Reset(m_command_allocators[m_frame_index].Get(), m_pipeline_state.Get()));

    // Set necessary state.
    m_command_list->SetPipelineState(m_pipeline_state.Get());
    m_command_list->SetGraphicsRootSignature(m_root_signature.Get());

    m_command_list->SetGraphicsRootConstantBufferView(Root_Parameter_CB, m_constant_buffer_gs->GetGPUVirtualAddress() + m_frame_index * sizeof(Constant_Buffer_GS));

    ID3D12DescriptorHeap* p_p_heaps[] = { m_srv_uav_heap.Get() };
    m_command_list->SetDescriptorHeaps(_countof(p_p_heaps), p_p_heaps);

    m_command_list->IASetVertexBuffers(0, 1, &m_vertex_buffer_view);
    m_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
    m_command_list->RSSetScissorRects(1, &m_scissor_rect);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_render_targets[m_frame_index].Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m_command_list->ResourceBarrier(1, &barrier);

    // Indicate that the back buffer will be used as a render target.
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = m_rtv_heap->GetCPUDescriptorHandleForHeapStart();
    rtv_handle.ptr += (SIZE_T)(m_frame_index * m_rtv_descriptor_size);
    m_command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);

    // Record commands.
    const float clearColor[] = { 0.0f, 0.0f, 0.1f, 0.0f };
    m_command_list->ClearRenderTargetView(rtv_handle, clearColor, 0, nullptr);

    // Render the particles.

    float viewport_height = static_cast<float>(static_cast<UINT>(m_viewport.Height) / m_height_instances);
    float viewport_width = static_cast<float>(static_cast<UINT>(m_viewport.Width) / m_width_instances);
    for (UINT n = 0; n < Thread_Count; ++n)
    {
        const UINT srv_index = n + (m_srv_index[n] == 0 ? Srv_Particle_Pos_Vel_0 : Srv_Particle_Pos_Vel_1);

        D3D12_VIEWPORT viewport = {};
        viewport.TopLeftX = (n % m_width_instances) * viewport_width;
        viewport.TopLeftY = (n / m_width_instances) * viewport_height;
        viewport.Width = viewport_width;
        viewport.Height = viewport_height;
        viewport.MinDepth = D3D12_MIN_DEPTH;
        viewport.MaxDepth = D3D12_MAX_DEPTH;
        m_command_list->RSSetViewports(1, &viewport);

        D3D12_GPU_DESCRIPTOR_HANDLE srv_handle = m_srv_uav_heap->GetGPUDescriptorHandleForHeapStart();
        srv_handle.ptr += (SIZE_T)(srv_index * m_srv_uav_descriptor_size);
        m_command_list->SetGraphicsRootDescriptorTable(Root_Parameter_SRV, srv_handle);

        PIXBeginEvent(m_command_list.Get(), 0, L"Draw particles for thread %u", n);
        m_command_list->DrawInstanced(Particle_Count, 1, 0, 0);
        PIXEndEvent(m_command_list.Get());
    }

    m_command_list->RSSetViewports(1, &m_viewport);

    // Indicate that the back buffer will now be used to present.
    barrier.Transition.pResource = m_render_targets[m_frame_index].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_command_list->ResourceBarrier(1, &barrier);

    ThrowIfFailed(m_command_list->Close());
}

DWORD D3D12RainDrop::AsyncComputeThreadProc(int thread_index)
{
    // Get the globle resource to local resource
    ID3D12CommandQueue* p_command_queue = m_compute_command_queue[thread_index].Get();
    ID3D12CommandAllocator* p_command_allocator = m_compute_command_allocator[thread_index].Get();
    ID3D12GraphicsCommandList* p_command_list = m_compute_command_list[thread_index].Get();
    ID3D12Fence* p_fence = m_thread_fences[thread_index].Get();

    while (0 == InterlockedGetValue(&m_terminating))
    {
        // Run the particle simulation.
        Simulate(thread_index);

        // Close and execute the command list.
        ThrowIfFailed(p_command_list->Close());
        ID3D12CommandList* p_p_command_lists[] = { p_command_list };

        PIXBeginEvent(p_command_queue, 0, L"Thread %d: Iterate on the particle simulation", thread_index);
        p_command_queue->ExecuteCommandLists(1, p_p_command_lists);
        PIXEndEvent(p_command_queue);

        // Wait for the compute shader to complete the simulation.
        UINT64 thread_fence_value = _InterlockedIncrement(&m_thread_fence_values[thread_index]);
        ThrowIfFailed(p_command_queue->Signal(p_fence, thread_fence_value));
        ThrowIfFailed(p_fence->SetEventOnCompletion(thread_fence_value, m_thread_fence_events[thread_index]));
        WaitForSingleObject(m_thread_fence_events[thread_index], INFINITE);

        // Wait for the render thread to be done with the SRV so that
        // the next frame in the simulation can run.
        UINT64 render_context_fence_value = InterlockedGetValue(&m_render_context_fence_values[thread_index]);
        if (m_render_context_fence->GetCompletedValue() < render_context_fence_value)
        {
            ThrowIfFailed(p_command_queue->Wait(m_render_context_fence.Get(), render_context_fence_value));
            InterlockedExchange(&m_render_context_fence_values[thread_index], 0);
        }

        // Swap the indices to the SRV and UAV.
        m_srv_index[thread_index] = 1 - m_srv_index[thread_index];

        // Prepare for the next frame.
        ThrowIfFailed(p_command_allocator->Reset());
        ThrowIfFailed(p_command_list->Reset(p_command_allocator, m_compute_state.Get()));
    }

    return 0;
}

// Run the particle simulation using the compute shader.
void D3D12RainDrop::Simulate(UINT thread_index)
{
    ID3D12GraphicsCommandList* p_command_list = m_compute_command_list[thread_index].Get();

    UINT srv_index;
    UINT uav_index;
    ID3D12Resource* p_uav_resource;

    // Flip the buffers
    if (m_srv_index[thread_index])
    {
        srv_index = Srv_Particle_Pos_Vel_0;
        uav_index = Uav_Particle_Pos_Vel_1;
        p_uav_resource = m_particle_buffer_1[thread_index].Get();
    }
    else
    {
        srv_index = Srv_Particle_Pos_Vel_1;
        uav_index = Uav_Particle_Pos_Vel_0;
        p_uav_resource = m_particle_buffer_0[thread_index].Get();
    }

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = p_uav_resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    p_command_list->ResourceBarrier(1, &barrier);

    p_command_list->SetPipelineState(m_compute_state.Get());
    p_command_list->SetComputeRootSignature(m_compute_root_signature.Get());

    ID3D12DescriptorHeap* p_p_heap[] = { m_srv_uav_heap.Get() };
    p_command_list->SetDescriptorHeaps(_countof(p_p_heap), p_p_heap);

    D3D12_GPU_DESCRIPTOR_HANDLE srv_handle = m_srv_uav_heap->GetGPUDescriptorHandleForHeapStart();
    srv_handle.ptr += (SIZE_T)((srv_index + thread_index) * m_srv_uav_descriptor_size);
    D3D12_GPU_DESCRIPTOR_HANDLE uav_handle = m_srv_uav_heap->GetGPUDescriptorHandleForHeapStart();
    uav_handle.ptr += (SIZE_T)((uav_index + thread_index) * m_srv_uav_descriptor_size);

    p_command_list->SetComputeRootConstantBufferView(Root_Parameter_CB, m_constant_buffer_cs->GetGPUVirtualAddress());
    p_command_list->SetComputeRootDescriptorTable(Root_Parameter_SRV, srv_handle);
    p_command_list->SetComputeRootDescriptorTable(Root_Parameter_UAV, uav_handle);

    p_command_list->Dispatch(static_cast<int>(ceil(Particle_Count / 128.0f)), 1, 1);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    p_command_list->ResourceBarrier(1, &barrier);
}

void D3D12RainDrop::OnDestory()
{
    // Notify the compute threads that the app is shutting down.
    InterlockedExchange(&m_terminating, 1);
    WaitForMultipleObjects(Thread_Count, m_thread_handles, TRUE, INFINITE);

    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForRenderContext();

    // Close handles to fence events and threads.
    CloseHandle(m_render_context_fence_event);
    for (int n = 0; n < Thread_Count; n++)
    {
        CloseHandle(m_thread_handles[n]);
        CloseHandle(m_thread_fence_events[n]);
    }
}

void D3D12RainDrop::OnKeyDown(UINT8 key)
{
    m_camera.OnKeyDown(key);
}

void D3D12RainDrop::OnKeyUp(UINT8 key)
{
    m_camera.OnKeyUp(key);
}

void D3D12RainDrop::WaitForRenderContext()
{
    // Add a signal command to the queue.
    ThrowIfFailed(m_command_queue->Signal(m_render_context_fence.Get(), m_render_context_fence_value));

    // Instruct the fence to set the event object when the signal command completes.
    ThrowIfFailed(m_render_context_fence->SetEventOnCompletion(m_render_context_fence_value, m_render_context_fence_event));

    ++m_render_context_fence_value;

    // Wait until the signal command has been processed.
    WaitForSingleObject(m_render_context_fence_event, INFINITE);
}

// Cycle through the frame resources. This method blocks execution if the 
// next frame resource in the queue has not yet had its previous contents 
// processed by the GPU.
void D3D12RainDrop::MoveToNextFrame()
{
    // Assign the current fence value to the current frame.
    m_frame_fence_values[m_frame_index] = m_render_context_fence_value;

    // Signal and increment the fence value.
    ThrowIfFailed(m_command_queue->Signal(m_render_context_fence.Get(), m_render_context_fence_value));
    m_render_context_fence_value++;

    // Update the frame index.
    m_frame_index = m_swap_chain->GetCurrentBackBufferIndex();

    // If the next frame is not ready to be rendered yet, wait until it is ready.
    if (m_render_context_fence->GetCompletedValue() < m_frame_fence_values[m_frame_index])
    {
        ThrowIfFailed(m_render_context_fence->SetEventOnCompletion(m_frame_fence_values[m_frame_index], m_render_context_fence_event));
        WaitForSingleObject(m_render_context_fence_event, INFINITE);
    }
}
