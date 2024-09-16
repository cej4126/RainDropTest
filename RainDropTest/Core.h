#pragma once

#include "stdafx.h"
#include "DXApp.h"
#include "Resources.h"
#include "Surface.h"
#include "Command.h"
#include "Camera.h"
#include "RainDrop.h"

// InterlockedCompareExchange returns the object's value if the 
// comparison fails.  If it is already 0, then its value won't 
// change and 0 will be returned.
#define InterlockedGetValue(object) InterlockedCompareExchange(object, 0, 0)

using Microsoft::WRL::ComPtr;
namespace core {

    struct frame_info
    {
        UINT* render_item_ids{ nullptr };
        float* thresholds{ nullptr };
        UINT render_item_count{ 0 };
        UINT camera_id{ Invalid_Index };
    };

    struct d3d12_frame_info
    {
        const frame_info* info{ nullptr };
        D3D12_GPU_VIRTUAL_ADDRESS global_shader_data{ 0 };
        UINT surface_width{ 0 };
        UINT surface_height{ 0 };
    };

    class Core : public dx_app
    {
    public:
        Core(UINT width, UINT height, std::wstring name);

        virtual void OnInit(UINT width, UINT height);
        virtual void run();
        virtual void render();
        virtual void OnRender();
        virtual void OnDestroy();
        virtual void create_surface(HWND hwnd, UINT width, UINT height);
        void OnUpdate(float dt);
        void remove_surface();

        id3d12_device* const device() const { return m_device.Get(); }
        ID3D12Fence* render_context_fence() { return m_render_context_fence.Get(); }
        UINT const current_frame_index() const { return m_frame_index; }
        void set_deferred_releases_flag() { deferred_releases_flag[m_frame_index] = true; }
        void deferred_release(IUnknown* resource);
        UINT64 get_render_context_fence_value() { return InterlockedGetValue(&m_render_context_fence_value1); }
        void set_render_context_fence_value() { InterlockedExchange(&m_render_context_fence_value1, 0); }

        Descriptor_Heap& rtv_heap() { return m_rtv_desc_heap; }
        Descriptor_Heap& dsv_heap() { return m_dsv_desc_heap; }
        Descriptor_Heap& srv_heap() { return m_srv_desc_heap; }
        Descriptor_Heap& uav_heap() { return m_uav_desc_heap; }
        constant_buffer& cbuffer() { return m_constant_buffers[current_frame_index()]; }

    private:

        rain_drop::RainDrop m_rain_drop;

        ComPtr<id3d12_device> m_device;
        ComPtr<ID3D12Resource> m_depth_stencil;
        ComPtr<ID3D12RootSignature> m_root_signature;
        ComPtr<ID3D12RootSignature> m_compute_root_signature;
        UINT m_frame_index;

        command::Command m_command;
        ComPtr<IDXGIFactory7> m_factory;

        UINT m_srv_index;  // Denotes which of the particle buffer resource views is the SRV(0 or 1).The UAV is 1 - srvIndex.
        UINT m_camera_id;

        // Synchronization objects.
        ComPtr<ID3D12Fence> m_render_context_fence;
        UINT64 m_render_context_fence_value{ 0 };
        HANDLE m_render_context_fence_event{ nullptr };
        UINT64 m_frame_fence_values[Frame_Count];

        UINT64 volatile m_render_context_fence_value1{ 0 };
        UINT64 volatile m_thread_fence_value{ 0 };

        bool deferred_releases_flag[Frame_Count]{ FALSE };
        std::mutex deferred_releases_mutex{};
        std::vector<IUnknown*> deferred_releases[Frame_Count]{};

        Descriptor_Heap m_rtv_desc_heap{ D3D12_DESCRIPTOR_HEAP_TYPE_RTV };
        Descriptor_Heap m_dsv_desc_heap{ D3D12_DESCRIPTOR_HEAP_TYPE_DSV };
        Descriptor_Heap m_srv_desc_heap{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };
        Descriptor_Heap m_uav_desc_heap{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };

        constant_buffer m_constant_buffers[Frame_Count];

        void LoadPipeline();
        void LoadAssets(UINT width, UINT height);
        void PopulateCommandList();

        void __declspec(noinline) process_deferred_releases(UINT frame_index);

        void WaitForRenderContext();
        void MoveToNextFrame();
    };

    bool initialize();
    void shutdown();
}