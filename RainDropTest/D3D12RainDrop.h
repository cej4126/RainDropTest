#pragma once

#include "stdafx.h"
#include "DXSample.h"
#include "SimpleCamera.h"
#include "StepTimer.h"

using namespace DirectX;

using Microsoft::WRL::ComPtr;

class D3D12RainDrop : public DXSample
{
public:
    D3D12RainDrop(UINT width, UINT height, std::wstring name);

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestory();
    virtual void OnKeyDown(UINT8 key);
    virtual void OnKeyUp(UINT8 key);

private:
    static const UINT Frame_Count = 2;
    static const UINT Thread_Count = 1;

    static const float Particle_Spread;
    static const UINT Particle_Count = 10000;

    // "Vertex" definition for particles. Triangle vertices are generated 
    // by the geometry shader. Color data will be assigned to those 
    // vertices via this struct.
    struct Particle_Vertex
    {
        XMFLOAT4 color;
    };

    // Position and velocity data for the particles in the system.
    // Two buffers full of Particle data are utilized in this sample.
    // The compute thread alternates writing to each of them.
    // The render thread renders using the buffer that is not currently
    // in use by the compute shader.
    struct Particle
    {
        XMFLOAT4 position;
        XMFLOAT4 velocity;
    };

    struct Constant_Buffer_GS
    {
        XMFLOAT4X4 world_view_projection;
        XMFLOAT4X4 inverse_view;

        // Constant buffers are 256-byte aligned in GPU memory. Padding is added
        // for convenience when computing the struct's size.
        float padding[32];
    };

    struct Constant_Buffer_CS
    {
        UINT param[4];
        float paramf[4];
    };

    // Pipeline objects
    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissor_rect;
    ComPtr<IDXGISwapChain3> m_swap_chain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_render_targets[Frame_Count];
    ComPtr<ID3D12CommandQueue> m_command_queue;
    ComPtr<ID3D12RootSignature> m_root_signature;
    ComPtr<ID3D12RootSignature> m_compute_root_signature;
    UINT m_frame_index;
    ComPtr<ID3D12CommandAllocator> m_command_allocators[Frame_Count];
    ComPtr<ID3D12DescriptorHeap> m_rtv_heap;
    ComPtr<ID3D12DescriptorHeap> m_srv_uav_heap;
    UINT m_rtv_descriptor_size;
    UINT m_srv_uav_descriptor_size;

    UINT8* m_pConstantBufferGSData;

    UINT m_srv_index[Thread_Count];		// Denotes which of the particle buffer resource views is the SRV (0 or 1). The UAV is 1 - srvIndex.
    UINT m_height_instances;
    UINT m_width_instances;

    // Asset objects.
    ComPtr<ID3D12PipelineState> m_pipeline_state;
    ComPtr<ID3D12PipelineState> m_compute_state;
    ComPtr<ID3D12GraphicsCommandList> m_command_list;
    ComPtr<ID3D12Resource> m_vertex_buffer;
    ComPtr<ID3D12Resource> m_vertex_buffer_upload;
    D3D12_VERTEX_BUFFER_VIEW m_vertex_buffer_view;
    ComPtr<ID3D12Resource> m_particle_buffer_0[Thread_Count];
    ComPtr<ID3D12Resource> m_particle_buffer_1[Thread_Count];
    ComPtr<ID3D12Resource> m_particle_buffer_upload_0[Thread_Count];
    ComPtr<ID3D12Resource> m_particle_buffer_upload_1[Thread_Count];
    ComPtr<ID3D12Resource> m_constant_buffer_gs;
    UINT8* m_p_constant_buffer_gs_data;
    ComPtr<ID3D12Resource> m_constant_buffer_cs;

    SimpleCamera m_camera;
    StepTimer m_timer;

    // Compute objects.
    ComPtr<ID3D12CommandQueue> m_compute_command_queue[Thread_Count];
    ComPtr<ID3D12CommandAllocator> m_compute_command_allocator[Thread_Count];
    ComPtr<ID3D12GraphicsCommandList> m_compute_command_list[Thread_Count];

    // Synchronization objects.
    HANDLE m_swap_chain_event;
    ComPtr<ID3D12Fence> m_render_context_fence;
    UINT64 m_render_context_fence_value;
    HANDLE m_render_context_fence_event;
    UINT64 m_frame_fence_values[Frame_Count];

    ComPtr<ID3D12Fence> m_thread_fences[Thread_Count];
    volatile HANDLE m_thread_fence_events[Thread_Count];

    // Thread state.
    LONG volatile m_terminating;
    UINT64 volatile m_render_context_fence_values[Thread_Count];
    UINT64 volatile m_thread_fence_values[Thread_Count];

    struct ThreadData
    {
        D3D12RainDrop* p_context;
        UINT thread_index;
    };
    ThreadData m_thread_data[Thread_Count];
    HANDLE m_thread_handles[Thread_Count];


    // Indices of shader resources in the descriptor heap.
    enum Root_Parameters : UINT32
    {
        Root_Parameter_CB = 0,
        Root_Parameter_SRV,
        Root_Parameter_UAV,

        Root_Parameters_Count
    };

    enum Descriptor_Heap_Index : UINT32
    {
        Uav_Particle_Pos_Vel_0 = 0,
        Uav_Particle_Pos_Vel_1 = Uav_Particle_Pos_Vel_0 + Thread_Count,
        Srv_Particle_Pos_Vel_0 = Uav_Particle_Pos_Vel_1 + Thread_Count,
        Srv_Particle_Pos_Vel_1 = Srv_Particle_Pos_Vel_0 + Thread_Count,
        Descriptor_Count = Srv_Particle_Pos_Vel_1 + Thread_Count
    };

    void LoadPipeline();
    void LoadAssets();
    UINT64 Update_Subresource(ID3D12Resource* p_destination_resource, ID3D12Resource* p_intermediate_resource, D3D12_SUBRESOURCE_DATA* p_src_data);
    void CreateVertexBuffer();
    void CreateAsyncContexts();
    float RandomPercent();
    void LoadParticles(_Out_writes_(number_of_paricles) Particle* p_particles, const XMFLOAT3& center, const XMFLOAT4& velocity, float spread, UINT number_of_paricles);
    void CreateParticleBuffers();
    void PopulateCommandList();

    static DWORD WINAPI ThreadProc(ThreadData* p_data)
    {
        return p_data->p_context->AsyncComputeThreadProc(p_data->thread_index);
    }

    DWORD AsyncComputeThreadProc(int thread_index);
    void Simulate(UINT thread_index);

    void WaitForRenderContext();
    void MoveToNextFrame();
};

