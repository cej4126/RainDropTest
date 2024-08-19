#include <functional>
#include <string>
#include "stdafx.h"
#include "Helpers.h"
#include "RainDrop.h"
#include "Command.h"
#include "Main.h"
#include "Barriers.h"
#include "Upload.h"
#include "Buffers.h"
#include "Utilities.h"
#include "ContentToEngine.h"
#include "Shaders.h"
#include <filesystem>
#include "Content.h"
#include "Transform.h"
#include "Entity.h"
#include "Shaders.h"
#include "Resources.h"
#include "GraphicPass.h"
#include "SharedTypes.h"
#include "FreeList.h"
#include "Input.h"
#include "Scripts.h"
#include "Core.h"

//extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 614; }
//extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

namespace d3d12::rain_drop {

    namespace {
        UINT cube_model_id{ Invalid_Index };
        UINT cube_entity_id{ Invalid_Index };
        UINT cube_item_id{ Invalid_Index };
        UINT material_id{ Invalid_Index };

        game_entity::entity camera_entity{};

        barriers::resource_barrier resource_barriers{};

        utl::vector<UINT> surface_ids;

        d3d12_frame_info get_d3d12_frame_info(const frame_info& info, constant_buffer& cbuffer, const surface::Surface& surface)
        {
            camera::Camera& camera{ camera::get(info.camera_id) };
            camera.update();
            hlsl::GlobalShaderData global_shader_data{};

            XMStoreFloat4x4A(&global_shader_data.View, camera.view());
            XMStoreFloat4x4A(&global_shader_data.Projection, camera.projection());
            XMStoreFloat4x4A(&global_shader_data.InverseProjection, camera.inverse_projection());
            XMStoreFloat4x4A(&global_shader_data.ViewProjection, camera.view_projection());
            XMStoreFloat4x4A(&global_shader_data.InverseViewProjection, camera.inverse_view_projection());
            XMStoreFloat3(&global_shader_data.CameraPosition, camera.position());
            XMStoreFloat3(&global_shader_data.CameraDirection, camera.direction());

            cbuffer.clear();
            hlsl::GlobalShaderData* const shader_data{ cbuffer.allocate<hlsl::GlobalShaderData>() };
            // TODO: handle the case when cbuffer is full.
            memcpy(shader_data, &global_shader_data, sizeof(hlsl::GlobalShaderData));

            d3d12_frame_info d3d12_info
            {
                &info,
                cbuffer.gpu_address(shader_data)
            };

            return d3d12_info;
        }
    } // anonymous namespace


    //const float RainDrop::Particle_Spread = 400.0f;
    const float RainDrop::Particle_Spread = 500.0f;

    RainDrop::RainDrop() :
        m_terminating(0)
    {}


    //temp
    void RainDrop::create_descriptor_heap()
    {
        // descriptor heap
        {
            // Describe and create a shader resource view (SRV) and unordered
            // access view (UAV) descriptor heap.
            D3D12_DESCRIPTOR_HEAP_DESC srv_uav_heap_desc = {};
            srv_uav_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;      // D3D12_DESCRIPTOR_HEAP_TYPE Type;
            srv_uav_heap_desc.NumDescriptors = Descriptor_Count;                  // UINT NumDescriptors;
            srv_uav_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;    // D3D12_DESCRIPTOR_HEAP_FLAGS Flags;
            srv_uav_heap_desc.NodeMask = 0;                        // UINT NodeMask;
            ThrowIfFailed(core::device()->CreateDescriptorHeap(&srv_uav_heap_desc, IID_PPV_ARGS(&m_srv_uav_heap)));
            NAME_D3D12_COMPTR_OBJECT(m_srv_uav_heap);

            m_srv_uav_descriptor_size = core::device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }
    }


    bool RainDrop::initialize(UINT width, UINT height)
    {
        m_width = width;
        m_height = height;

        m_particle_src_index = 0;
        //ZeroMemory(m_frame_fence_values, sizeof(m_frame_fence_values));

        //m_render_context_fence_value1 = 0;
        m_thread_fence_value = 0;

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
            D3D12_INPUT_ELEMENT_DESC input_element_descriptors[] =
            {
                { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            };

            // Describe and create the graphics pipeline state object (PSO).
            D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
            pso_desc.pRootSignature = m_root_signature.Get();                                              // ID3D12RootSignature* pRootSignature;
            pso_desc.VS = shaders::get_engine_shader(shaders::engine_shader::particle_draw_vs);            // D3D12_SHADER_BYTECODE VS;
            pso_desc.PS = shaders::get_engine_shader(shaders::engine_shader::particle_draw_ps);            // D3D12_SHADER_BYTECODE PS;
            pso_desc.DS = {};                                                                              // D3D12_SHADER_BYTECODE DS;
            pso_desc.HS = {};                                                                              // D3D12_SHADER_BYTECODE HS;
            pso_desc.GS = shaders::get_engine_shader(shaders::engine_shader::particle_draw_gs);            // D3D12_SHADER_BYTECODE GS;
            pso_desc.StreamOutput = {};                                                                    // D3D12_STREAM_OUTPUT_DESC StreamOutput;
            pso_desc.BlendState = d3dx::blend_state.blend_desc;                                            // D3D12_BLEND_DESC BlendState;
            pso_desc.SampleMask = UINT_MAX;                                                                // UINT SampleMask;
            pso_desc.RasterizerState = d3dx::rasterizer_state.face_cull;                                   // D3D12_RASTERIZER_DESC RasterizerState;
            // depth test - pso_desc.DepthStencilState = d3dx::depth_desc_state.enable;                                    // D3D12_DEPTH_STENCIL_DESC DepthStencilState;
            //pso_desc.DepthStencilState = d3dx::depth_desc_state.enable;                                    // D3D12_DEPTH_STENCIL_DESC DepthStencilState;
            pso_desc.DepthStencilState = d3dx::depth_desc_state.reversed;                                    // D3D12_DEPTH_STENCIL_DESC DepthStencilState;
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

            ThrowIfFailed(core::device()->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&m_pipeline_state)));
            NAME_D3D12_COMPTR_OBJECT(m_pipeline_state);

            // Describe and create the compute pipeline state object (PSO).
            D3D12_COMPUTE_PIPELINE_STATE_DESC compute_pso_desc = {};
            compute_pso_desc.pRootSignature = m_compute_root_signature.Get();                              // ID3D12RootSignature* pRootSignature;
            compute_pso_desc.CS = shaders::get_engine_shader(shaders::engine_shader::n_body_gravity_cs);   // D3D12_SHADER_BYTECODE CS;
            compute_pso_desc.NodeMask = 0;                                                                 // UINT NodeMask;
            compute_pso_desc.CachedPSO = {};                                                               // D3D12_CACHED_PIPELINE_STATE CachedPSO;
            compute_pso_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;                                       // D3D12_PIPELINE_STATE_FLAGS Flags;

            ThrowIfFailed(core::device()->CreateComputePipelineState(&compute_pso_desc, IID_PPV_ARGS(&m_compute_state)));
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

            //command_list->Close();
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

            ThrowIfFailed(core::device()->CreateCommittedResource(&d3dx::heap_properties.upload_heap, D3D12_HEAP_FLAG_NONE, &buffer_desc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_constant_buffer_gs)));
            NAME_D3D12_COMPTR_OBJECT(m_constant_buffer_gs);

            D3D12_RANGE read_range = {}; // We do not intend to read from this resource on the CPU.
            read_range.Begin = 0;
            read_range.End = 0;
            ThrowIfFailed(m_constant_buffer_gs->Map(0, &read_range, reinterpret_cast<void**>(&m_p_constant_buffer_gs_data)));
            ZeroMemory(m_p_constant_buffer_gs_data, constant_buffer_gs_size);
        }

        //CreateAsyncContexts();
        return true;
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

        //command_list->Close();
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
            p_particles[i].position.y = -10.0f + (RandomPercent()) * 5.f;


            p_particles[i].position.z = center.z + RandomPercent() * spread;
            p_particles[i].position.w = p_particles[i].position.y;

            p_particles[i].velocity.x = RandomPercent() * 0.000005f;
            p_particles[i].velocity.y = 0.0001f + RandomPercent() * 0.00005f;

            p_particles[i].velocity.z = RandomPercent() * 0.000005f;
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
        float center_spread = 10.0f;
        float speed = 0.00001f;

        const UINT number_of_objects = 5;
        //                      center             vel    spread
        LoadParticles(&data[0], XMFLOAT3(0, 0, 0), speed, center_spread, Particle_Count - number_of_objects);

        speed = 0.00001f;
        center_spread = 10.f;
        const UINT start_index = Particle_Count - number_of_objects;
        LoadParticles(&data[start_index], XMFLOAT3(0, 0, 0), speed, center_spread, number_of_objects);

        //command_list->Close();
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

        core::device()->CreateShaderResourceView(m_particle_buffer_0.Get(), &srv_desc, srv_handle_0);
        core::device()->CreateShaderResourceView(m_particle_buffer_1.Get(), &srv_desc, srv_handle_1);

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

        core::device()->CreateUnorderedAccessView(m_particle_buffer_0.Get(), nullptr, &uav_desc, uav_handle_0);
        core::device()->CreateUnorderedAccessView(m_particle_buffer_1.Get(), nullptr, &uav_desc, uav_handle_1);
    }

    void RainDrop::update(UINT camera_id, UINT frame_index)
    {
        camera::Camera& camera{ camera::get(camera_id) };

        Constant_Buffer_GS constant_buffer_gs = {};
        //XMStoreFloat4x4(&constant_buffer_gs.world_view_projection, XMMatrixMultiply(m_camera.GetViewMatrix(), m_camera.GetProjectionMatrix(0.8f, m_aspect_ratio, 1.0f, 5000.0f)));
        //XMStoreFloat4x4(&constant_buffer_gs.inverse_view, XMMatrixInverse(nullptr, m_camera.GetViewMatrix()));


        XMStoreFloat4x4(&constant_buffer_gs.world_view_projection, camera.view_projection());
        XMStoreFloat4x4(&constant_buffer_gs.inverse_view, XMMatrixInverse(nullptr, camera.view()));

        UINT8* destination = m_p_constant_buffer_gs_data + sizeof(Constant_Buffer_GS) * frame_index;
        memcpy(destination, &constant_buffer_gs, sizeof(Constant_Buffer_GS));

    }

    void RainDrop::CreateAsyncContexts()
    {
        // Create the compute resources.
        D3D12_COMMAND_QUEUE_DESC queue_desc = { D3D12_COMMAND_LIST_TYPE_COMPUTE, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0 };
        ThrowIfFailed(core::device()->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_compute_command_queue)));
        ThrowIfFailed(core::device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_compute_command_allocator)));
        ThrowIfFailed(core::device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_compute_command_allocator.Get(), nullptr, IID_PPV_ARGS(&m_compute_command_list)));
        ThrowIfFailed(core::device()->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_thread_fence)));

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

    void RainDrop::sync_compute_tread(ID3D12CommandQueue* command_queue)
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
            ThrowIfFailed(command_queue->Wait(m_thread_fence.Get(), thread_fence_value));
        }
    }

    void RainDrop::populate_command_list(id3d12_graphics_command_list* command_list, UINT frame_index)
    {
        // Set necessary state.
        command_list->SetPipelineState(m_pipeline_state.Get());
        command_list->SetGraphicsRootSignature(m_root_signature.Get());

        command_list->SetGraphicsRootConstantBufferView(Root_Parameter_CB, m_constant_buffer_gs->GetGPUVirtualAddress() + frame_index * sizeof(Constant_Buffer_GS));

        ID3D12DescriptorHeap* p_p_heaps[] = { m_srv_uav_heap.Get() };
        command_list->SetDescriptorHeaps(_countof(p_p_heaps), p_p_heaps);

        command_list->IASetVertexBuffers(0, 1, &m_vertex_buffer_view);
        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
    }

    void RainDrop::render(id3d12_graphics_command_list* command_list)
    {
        // Render the particles.
        const UINT srv_index = (m_particle_src_index == 0 ? Srv_Particle_Pos_Vel_0 : Srv_Particle_Pos_Vel_1);

        D3D12_GPU_DESCRIPTOR_HANDLE srv_handle = m_srv_uav_heap->GetGPUDescriptorHandleForHeapStart();
        srv_handle.ptr += (SIZE_T)(srv_index * m_srv_uav_descriptor_size);
        command_list->SetGraphicsRootDescriptorTable(Root_Parameter_SRV, srv_handle);

        //PIXBeginEvent(command_list, 0, L"Draw particles for thread");
        command_list->DrawInstanced(Particle_Count, 1, 0, 0);
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
            UINT64 render_context_fence_value = core::get_render_context_fence_value();
            if (core::render_context_fence()->GetCompletedValue() < render_context_fence_value)
            {
                ThrowIfFailed(p_command_queue->Wait(core::render_context_fence(), render_context_fence_value));
                InterlockedExchange(&m_render_context_fence_value1, 0);
            }

            // Swap the indices to the SRV and UAV.
            m_particle_src_index = 1 - m_particle_src_index;

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
        if (m_particle_src_index)
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

    void remove_item(UINT& entity_id, UINT& item_id, UINT& model_id)
    {
        if (entity_id != Invalid_Index)
        {
            game_entity::remove(entity_id);
            entity_id = Invalid_Index;
        }

        if (item_id != Invalid_Index)
        {
            content::render_item::remove(item_id);
            item_id = Invalid_Index;
        }

        if (model_id != Invalid_Index)
        {
            content::destroy_resource(model_id, content::asset_type::mesh);
            model_id = Invalid_Index;
        }
    }

    void RainDrop::shutdown()
    {
        // Notify the compute threads that the app is shutting down.
        InterlockedExchange(&m_terminating, 1);
        WaitForMultipleObjects(1, &m_thread_handle, TRUE, INFINITE);
    }
}