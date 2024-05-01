#pragma once
#include "stdafx.h"
#include "Resources.h"

class Descriptor_Heap;

namespace d3d12::core {
    template<typename T>
    constexpr void release(T*& resource)
    {
        if (resource)
        {
            resource->Release();
            resource = nullptr;
        }
    }

    id3d12_device* const device();
    UINT current_frame_index();
    void set_deferred_release_flag();

    void main_deferred_release(IUnknown* resource);

    template<typename T>
    constexpr void deferred_release(T*& resource)
    {
        if (resource)
        {
            main_deferred_release(resource);
            resource = nullptr;
        }
    }


    [[nodiscard]] Descriptor_Heap& dsv_heap();
    [[nodiscard]] Descriptor_Heap& rtv_heap();
    [[nodiscard]] Descriptor_Heap& srv_heap();
    [[nodiscard]] Descriptor_Heap& uav_heap();
}
