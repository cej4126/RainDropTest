#pragma once
#include "stdafx.h"
#include "Shaders.h"
#include "ContentToEngine.h"

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

    namespace material {

        struct material_header
        {
            content::material_type::type type;
            shaders::shader_flags::flags flags;
            UINT root_signature_id;
            UINT texture_count;
            UINT shader_count;
        };

        struct materials_cache
        {
            ID3D12RootSignature** const root_signatures;
            material_type::type* const material_types;
            UINT** const descriptor_indices;
            UINT* const texture_count;
            UINT* shader_count;
        };

//        return content::material::add(*(const d3d12::content::material_init_info* const)data);

        UINT add(d3d12::content::material_init_info info);
        void remove(UINT id);
        void get_materials(const UINT* const material_ids, UINT material_count, const materials_cache& cache, UINT descriptor_index_count);
    } // namespace material
}
