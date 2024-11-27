#pragma once

#include "stdafx.h"
#include "DXApp.h"
#include "Resources.h"
#include "Surface.h"
#include "Command.h"
#include "Camera.h"
#include "RainDrop.h"

using Microsoft::WRL::ComPtr;

namespace core {

    struct frame_info
    {
        UINT* render_item_ids{ nullptr };
        float* thresholds{ nullptr };
        UINT64 light_set_key{ 0 };
        float last_frame_time{ 16.7f };
        float average_frame_time{ 16.7f };
        UINT render_item_count{ 0 };
        UINT camera_id{ Invalid_Index };
    };

    struct d3d12_frame_info
    {
        const frame_info* info{ nullptr };
        camera::Camera* camera{ nullptr };
        D3D12_GPU_VIRTUAL_ADDRESS global_shader_data{ 0 };
        UINT surface_width{ 0 };
        UINT surface_height{ 0 };
        UINT light_id{ Invalid_Index };
        UINT frame_index{ 0 };
        float delta_time{ 16.7f };
    };


    ID3D12Fence* render_context_fence();

    bool initialize();
    void shutdown();

    template<typename T>
    constexpr void release(T*& resource)
    {
        if (resource)
        {
            resource->Release();
            resource = nullptr;
        }
    }

    namespace detail {
        void deferred_release(IUnknown* resource);
    }

    template<typename T>
    constexpr void deferred_release(T*& resource)
    {
        if (resource)
        {
            detail::deferred_release(resource);
            resource = nullptr;
        }
    }

    [[nodiscard]] id3d12_device* const device();
    [[nodiscard]] IDXGIFactory7* const factory();
    [[nodiscard]] ID3D12CommandQueue* const command_queue();
    [[nodiscard]] resource::Descriptor_Heap& rtv_heap();
    [[nodiscard]] resource::Descriptor_Heap& dsv_heap();
    [[nodiscard]] resource::Descriptor_Heap& srv_heap();
    [[nodiscard]] resource::Descriptor_Heap& uav_heap();
    [[nodiscard]] resource::constant_buffer& cbuffer();
    [[nodiscard]] UINT current_frame_index();
    void set_deferred_releases_flag();
    void render(UINT id, frame_info info);
    void flush();
}