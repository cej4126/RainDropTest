#pragma once
#include "stdafx.h"
#include "Entity.h"
#include "FreeList.h"
#include "SharedTypes.h"
#include "Core.h"
#include "Barriers.h"

namespace lights
{

    struct light_type {
        enum type : UINT32
        {
            directional,
            point,
            spot,

            count
        };
    };

    struct spot_light_params
    {
        // Umbra angle in radians [0, pi)
        float         umbra{};
        // Penumbra angle in radians [umbra, pi)
        float         penumbra{};
    };

    struct light_init_info
    {
        UINT64 set_key{ 0 };
        UINT32 entity_id{ Invalid_Index };
        light_type::type type{};
        float intensity{ 1.f };
        XMFLOAT3 color{ 1.f, 1.f, 1.f };
        // point and spot only
        XMFLOAT3 attenuation;
        float range;
        // spot only
        spot_light_params spot_params;
    };

    struct light_owner
    { 
        UINT32 entity_id{ Invalid_Index };
        UINT32 data_index{ Invalid_Index };
        light_type::type type;
        bool is_enabled;
    };

    class light_set
    {
    public:
    private:
        utl::free_list<light_owner> m_owners;
        utl::vector<hlsl::DirectionalLightParameters> m_non_cullable_lights;
        utl::vector<UINT32> m_non_cullable_owners;
    };

    void generate_lights();
    void update_light_buffers(core::d3d12_frame_info d3d12_info);
    void cull_lights(id3d12_graphics_command_list* cmd_list, core::d3d12_frame_info d3d12_info, barriers::resource_barrier barriers);

    D3D12_GPU_VIRTUAL_ADDRESS non_cullable_light_buffer(UINT frame_index);
    D3D12_GPU_VIRTUAL_ADDRESS cullable_light_buffer(UINT frame_index);
    D3D12_GPU_VIRTUAL_ADDRESS culling_info_buffer(UINT frame_index);

    D3D12_GPU_VIRTUAL_ADDRESS frustums(UINT light_culling_id, UINT frame_index);
    D3D12_GPU_VIRTUAL_ADDRESS light_grid_opaque(UINT light_culling_id, UINT frame_index);
    D3D12_GPU_VIRTUAL_ADDRESS light_index_list_opaque(UINT light_culling_id, UINT frame_index);
}
