#pragma once
#include "stdafx.h"

namespace d3d12::buffers {

    ID3D12Resource* create_buffer_default_with_upload(const void* data, UINT size, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

}
