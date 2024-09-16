#pragma once

#include "stdafx.h"
#include "DXApp.h"
#include "Resources.h"
#include "Surface.h"
#include "Command.h"
#include "Camera.h"

using Microsoft::WRL::ComPtr;
namespace d3d12::rain_drop {

    class RainDrop
    {
    public:
        RainDrop() = default;

        bool initialize(UINT width, UINT height);
        void create_descriptor_heap();
        void populate_command_list(id3d12_graphics_command_list* command_list, UINT frame_count);
        void update(UINT camera_id, UINT frame_index);

        void CreateAsyncContexts();
        void sync_compute_tread(ID3D12CommandQueue* command_queue);
        void render(id3d12_graphics_command_list* command_list);

        void shutdown();

        void set_rain_state(bool state) { m_rain_on = state; }

    private:
        static const float Particle_Spread;
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

        struct Constant_Buffer_CS
        {
            UINT param[4];
            float param_float[4];
        };

        ComPtr<ID3D12RootSignature> m_root_signature;
        ComPtr<ID3D12RootSignature> m_compute_root_signature;


        UINT m_particle_src_index{ 0 };  // Denotes which of the particle buffer resource views is the SRV(0 or 1).The UAV is 1 - srvIndex.

        // Asset objects.
        ComPtr<ID3D12PipelineState> m_pipeline_state;
        ComPtr<ID3D12PipelineState> m_compute_state;
        ComPtr<ID3D12Resource> m_vertex_buffer;
        D3D12_VERTEX_BUFFER_VIEW m_vertex_buffer_view;
        ComPtr<ID3D12Resource> m_particle_buffer_0;
        ComPtr<ID3D12Resource> m_particle_buffer_1;
        ComPtr<ID3D12Resource> m_constant_buffer_gs;
        UINT8* m_p_constant_buffer_gs_data{ nullptr };
        ComPtr<ID3D12Resource> m_constant_buffer_cs;
        ComPtr<ID3D12DescriptorHeap> m_srv_uav_heap;
        UINT m_srv_uav_descriptor_size{ 0 };

        // Compute objects.
        ComPtr<ID3D12CommandQueue> m_compute_command_queue;
        ComPtr<ID3D12CommandAllocator> m_compute_command_allocator;
        ComPtr<id3d12_graphics_command_list> m_compute_command_list;

        // Synchronization objects.
        UINT64 m_render_context_fence_value{ 0 };

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

        // Indices of shader resources in the descriptor heap.
        enum Root_Parameters : UINT
        {
            Root_Parameter_CB = 0,
            Root_Parameter_SRV,
            Root_Parameter_UAV,

            Root_Parameters_Count
        };

        enum Descriptor_Heap_Index : UINT
        {
            Uav_Particle_Pos_Vel_0 = 0,
            Uav_Particle_Pos_Vel_1 = Uav_Particle_Pos_Vel_0 + 1,
            Srv_Particle_Pos_Vel_0 = Uav_Particle_Pos_Vel_1 + 1,
            Srv_Particle_Pos_Vel_1 = Srv_Particle_Pos_Vel_0 + 1,

            Descriptor_Count = Srv_Particle_Pos_Vel_1 + 1
        };

        void CreateVertexBuffer();
        float RandomPercent();
        void LoadParticles(Particle* p_particles, const XMFLOAT3& center, const float& velocity, float spread, UINT number_of_paricles);
        void CreateParticleBuffers();

        static DWORD WINAPI ThreadProc(ThreadData* p_data)
        {
            return p_data->p_context->AsyncComputeThreadProc(p_data->thread_index);
        }

        DWORD AsyncComputeThreadProc(int thread_index);
        void Simulate();

        bool m_rain_on{ false };

     };
}