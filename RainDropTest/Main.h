 #pragma once
#include "stdafx.h"
#include "Resources.h"

class Descriptor_Heap;

namespace resource {
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
    ID3D12Fence* render_context_fence();
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
}
