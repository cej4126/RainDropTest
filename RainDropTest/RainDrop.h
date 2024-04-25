#pragma once

#include "stdafx.h"
#include "DXSample.h"
#include "SimpleCamera.h"
#include "StepTimer.h"
#include "Resources.h"
#include "Surface.h"
#include "Command.h"

using namespace DirectX;

using Microsoft::WRL::ComPtr;

class RainDrop : public DXSample
{
public:
    RainDrop(UINT width, UINT height, std::wstring name);

    virtual void OnInit();
    virtual void OnUpdate(float dt);
    virtual void OnRender();
    virtual void OnDestroy();
    virtual void OnKeyDown(UINT8 key);
    virtual void OnKeyUp(UINT8 key);

    virtual void create_surface(HWND hwnd, UINT width, UINT height);

    id3d12_device* const device() const { return m_device.Get(); }
    UINT const current_frame_index() const { return m_frame_index; }
    void set_deferred_releases_flag() { deferred_releases_flag[m_frame_index] = true; }
    void deferred_release(IUnknown* resource);

    Descriptor_Heap& rtv_heap() { return m_rtv_desc_heap; }
    Descriptor_Heap& dsv_heap() { return m_dsv_desc_heap; }
    Descriptor_Heap& srv_heap() { return m_srv_desc_heap; }
    Descriptor_Heap& uav_heap() { return m_uav_desc_heap; }


private:
    static const float Particle_Spread;
    //static const UINT Particle_Count = 10000;
    static const UINT Particle_Count = 1000;

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
        // for convenience when computing the structure's size.
        float padding[32];
    };

    struct Constant_Buffer_CS
    {
        UINT param[4];
        float param_float[4];
    };

    // Pipeline objects
    Surface m_surface;
    Render_Target render_target;

    ComPtr<id3d12_device> m_device;
    ComPtr<ID3D12Resource> m_depth_stencil;
    ComPtr<ID3D12RootSignature> m_root_signature;
    ComPtr<ID3D12RootSignature> m_compute_root_signature;
    UINT m_frame_index;

    Command m_command;
    ComPtr<IDXGIFactory7> m_factory;

    //UINT8* m_pConstantBufferGSData;

    UINT m_srv_index;  // Denotes which of the particle buffer resource views is the SRV(0 or 1).The UAV is 1 - srvIndex.

    // Asset objects.
    ComPtr<ID3D12PipelineState> m_pipeline_state;
    ComPtr<ID3D12PipelineState> m_compute_state;
    ComPtr<ID3D12Resource> m_vertex_buffer;
    ComPtr<ID3D12Resource> m_vertex_buffer_upload;
    D3D12_VERTEX_BUFFER_VIEW m_vertex_buffer_view;
    ComPtr<ID3D12Resource> m_particle_buffer_0;
    ComPtr<ID3D12Resource> m_particle_buffer_1;
    ComPtr<ID3D12Resource> m_particle_buffer_upload_0;
    ComPtr<ID3D12Resource> m_particle_buffer_upload_1;
    ComPtr<ID3D12Resource> m_constant_buffer_gs;
    UINT8* m_p_constant_buffer_gs_data{ nullptr };
    ComPtr<ID3D12Resource> m_constant_buffer_cs;
    ComPtr<ID3D12DescriptorHeap> m_rtv_heap;
    ComPtr<ID3D12DescriptorHeap> m_dsv_heap;
    ComPtr<ID3D12DescriptorHeap> m_srv_uav_heap;
    UINT m_rtv_descriptor_size;
    UINT m_srv_uav_descriptor_size;

    SimpleCamera m_camera;

    // Compute objects.
    ComPtr<ID3D12CommandQueue> m_compute_command_queue;
    ComPtr<ID3D12CommandAllocator> m_compute_command_allocator;
    ComPtr<id3d12_graphics_command_list> m_compute_command_list;

    // Synchronization objects.
    ComPtr<ID3D12Fence> m_render_context_fence;
    UINT64 m_render_context_fence_value{ 0 };
    HANDLE m_render_context_fence_event{ nullptr };
    UINT64 m_frame_fence_values[Frame_Count];

    ComPtr<ID3D12Fence> m_thread_fence;
    volatile HANDLE m_thread_fence_event{};

    // Thread state.
    LONG volatile m_terminating{ 0 };
    UINT64 volatile m_render_context_fence_value1{ 0 };
    UINT64 volatile m_thread_fence_value{ 0 };

    struct ThreadData
    {
        RainDrop* p_context;
        UINT thread_index;
    };
    ThreadData m_thread_data;
    HANDLE m_thread_handle{ nullptr };

    bool deferred_releases_flag[Frame_Count]{ FALSE };
    std::mutex deferred_releases_mutex{};
    std::vector<IUnknown*> deferred_releases[Frame_Count]{};

    //D3D12DescriptorHeap x;
    Descriptor_Heap m_rtv_desc_heap{ D3D12_DESCRIPTOR_HEAP_TYPE_RTV };
    Descriptor_Heap m_dsv_desc_heap{ D3D12_DESCRIPTOR_HEAP_TYPE_DSV };
    Descriptor_Heap m_srv_desc_heap{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };
    Descriptor_Heap m_uav_desc_heap{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };

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
        Uav_Particle_Pos_Vel_1 = Uav_Particle_Pos_Vel_0 + 1,
        Srv_Particle_Pos_Vel_0 = Uav_Particle_Pos_Vel_1 + 1,
        Srv_Particle_Pos_Vel_1 = Srv_Particle_Pos_Vel_0 + 1,

        Descriptor_Count = Srv_Particle_Pos_Vel_1 + 1
    };

    void LoadPipeline();
    void LoadAssets();
    void CreateVertexBuffer();
    void CreateAsyncContexts();
    float RandomPercent();
    //void LoadParticles(_Out_writes_(number_of_particles) Particle* p_particles, const XMFLOAT3& center, const XMFLOAT4& velocity, float spread, UINT number_of_particles);
    void LoadParticles(Particle* p_particles, const XMFLOAT3& center, const float& velocity, float spread, UINT number_of_paricles);
    void CreateParticleBuffers();
    void PopulateCommandList();

    void __declspec(noinline) process_deferred_releases(UINT frame_index);

    static DWORD WINAPI ThreadProc(ThreadData* p_data)
    {
        return p_data->p_context->AsyncComputeThreadProc(p_data->thread_index);
    }

    DWORD AsyncComputeThreadProc(int thread_index);
    void Simulate();

    void WaitForRenderContext();
    void MoveToNextFrame();
};
