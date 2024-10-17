#pragma once
#include "stdafx.h"

namespace core {
    struct d3d12_frame_info;
}

namespace post_process {

    bool initialize();
    void shutdown();

    void post_process(id3d12_graphics_command_list* cmd_list, const core::d3d12_frame_info& info, D3D12_CPU_DESCRIPTOR_HANDLE target_rtv);
}
