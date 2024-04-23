#include "Barriers.h"

namespace barriers {

    void transition_resource(
        id3d12_graphics_command_list* command_list, ID3D12Resource* resource,
        D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after,
        D3D12_RESOURCE_BARRIER_FLAGS flags /* = D3D12_RESOURCE_BARRIER_FLAG_NONE */,
        UINT subresource /* = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES */)
    {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = flags;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        barrier.Transition.Subresource = subresource;

        command_list->ResourceBarrier(1, &barrier);
    }
}
