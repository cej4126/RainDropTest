#pragma once
#include "stdafx.h"

namespace barriers {
    class resource_barrier
    {
    public:
        constexpr static UINT m_max_resource_barriers{ 32 };

        // D3D12_RESOURCE_BARRIER_TYPE_TRANSITION
        constexpr void add(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after,
            D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE, UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
        {
            assert(resource);
            assert(m_offset < m_max_resource_barriers);
            D3D12_RESOURCE_BARRIER& barrier{ m_barriers[m_offset] };
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = flags;
            barrier.Transition.pResource = resource;
            barrier.Transition.StateBefore = before;
            barrier.Transition.StateAfter = after;
            barrier.Transition.Subresource = subresource;

            ++m_offset;
        }

        // D3D12_RESOURCE_BARRIER_TYPE_UAV
        constexpr void add(ID3D12Resource* resource, D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE)
        {
            assert(resource);
            assert(m_offset < m_max_resource_barriers);
            D3D12_RESOURCE_BARRIER& barrier{ m_barriers[m_offset] };
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            barrier.Flags = flags;
            barrier.UAV.pResource = resource;

            ++m_offset;
        }

        // D3D12_RESOURCE_BARRIER_TYPE_ALIASING
        constexpr void add(ID3D12Resource* resource_before, ID3D12Resource* resource_after,
            D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE)
        {
            assert(resource_before && resource_after);
            assert(m_offset < m_max_resource_barriers);
            D3D12_RESOURCE_BARRIER& barrier{ m_barriers[m_offset] };
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
            barrier.Flags = flags;
            barrier.Aliasing.pResourceBefore = resource_before;
            barrier.Aliasing.pResourceAfter = resource_after;

            ++m_offset;
        }

        void apply(id3d12_graphics_command_list* command_list)
        {
            assert(m_offset);
            command_list->ResourceBarrier(m_offset, m_barriers);
            m_offset = 0;
        }

    private:
        D3D12_RESOURCE_BARRIER m_barriers[m_max_resource_barriers];
        UINT m_offset{ 0 };
    };

    void transition_resource(
        id3d12_graphics_command_list* command_list, ID3D12Resource* resource,
        D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after,
        D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
}
