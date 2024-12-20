#pragma once
#include "stdafx.h"
#include "Math.h"

namespace buffers {

    ID3D12Resource* create_buffer(const void* data, UINT data_size, bool is_cpu_accessible, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
    ID3D12Resource* create_buffer_default_with_upload(const void* data, UINT size, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
    UINT64 align_size_for_constant_buffer(UINT64 size);
    ID3D12Resource* create_buffer_default_without_upload(UINT size);
}
