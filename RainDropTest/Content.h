#pragma once
#include "stdafx.h"

namespace d3d12::content
{
    bool initialize();
    void shutdown();

    namespace sub_mesh {
        struct views_cache
        {
            D3D12_GPU_VIRTUAL_ADDRESS* const position_buffers;
            D3D12_GPU_VIRTUAL_ADDRESS* const element_buffers;
            D3D12_INDEX_BUFFER_VIEW* const index_buffer_views;
            D3D_PRIMITIVE_TOPOLOGY* const primitive_topologies;
            UINT* const elements_types;
        };

        UINT add(const UINT8*& data);
        void remove(UINT id);
        void get_views(const UINT* const gpu_ids, UINT id_count, const views_cache& cache);
    } // namespace sub_mesh
}
