#pragma once
#include "stdafx.h"

class Descriptor_Heap;

namespace core {
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
    constexpr void deferred_release_item(T*& resource)
    {
        if (resource)
        {
            deferred_release_item(resource);
            resource = nullptr;
        }
    }


    Descriptor_Heap& dsv_heap();
    Descriptor_Heap& rtv_heap();
    Descriptor_Heap& srv_heap();
    Descriptor_Heap& uav_heap();
}
