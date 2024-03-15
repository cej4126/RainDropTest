#pragma once
#include "stdafx.h"

class D3D12DescriptorHeap;

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

    D3D12DescriptorHeap& rtv_heap();
}
