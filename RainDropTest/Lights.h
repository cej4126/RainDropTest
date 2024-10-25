#pragma once
#include "stdafx.h"
#include "Entity.h"
#include "FreeList.h"
#include "SharedTypes.h"
#include "Core.h"
#include "Barriers.h"
#include "Resources.h"

namespace lights
{
    constexpr UINT light_culling_tile_size{ 32 };

    struct spot_light_params
    {
        // Umbra angle in radians [0, pi)
        float         umbra{};
        // Penumbra angle in radians [umbra, pi)
        float         penumbra{};
    };

    struct light_type {
        enum type : UINT32
        {
            directional,
            point,
            spot,

            count
        };
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
        bool is_enabled{ true };
    };

    struct light_owner
    { 
        UINT32 entity_id{ Invalid_Index };
        UINT32 light_index{ Invalid_Index }; // data_index
        light_type::type type;
        bool is_enabled;
    };
 
    class Light
    {
    public:

        constexpr explicit Light(UINT id, UINT64 light_set_key) : m_light_set_key{ light_set_key }, m_id{ id } {}
        constexpr Light() = default;
        constexpr UINT get_id() const { return m_id; }
        constexpr UINT64 get_sst_key() const { return m_light_set_key; }
        constexpr bool is_valid() const { return m_id != Invalid_Index; }

    private:


        UINT64 m_light_set_key{ 0 };
        UINT m_id{ Invalid_Index };
    };


    bool initialize();
    void shutdown();

    void generate_lights();

    [[nodiscard]] UINT add_cull_light();
    void remove_cull_light();

    void update_light_buffers(core::d3d12_frame_info d3d12_info);
    void cull_lights(id3d12_graphics_command_list* cmd_list, core::d3d12_frame_info d3d12_info, barriers::resource_barrier barriers);

    D3D12_GPU_VIRTUAL_ADDRESS non_cullable_light_buffer(UINT frame_index);
    D3D12_GPU_VIRTUAL_ADDRESS cullable_light_buffer(UINT frame_index);
    D3D12_GPU_VIRTUAL_ADDRESS culling_info_buffer(UINT frame_index);
    D3D12_GPU_VIRTUAL_ADDRESS bounding_sphere_buffer(UINT frame_index);

    D3D12_GPU_VIRTUAL_ADDRESS frustums(UINT light_culling_id, UINT frame_index);
    D3D12_GPU_VIRTUAL_ADDRESS light_grid_opaque(UINT light_culling_id, UINT frame_index);
    D3D12_GPU_VIRTUAL_ADDRESS light_index_list_opaque(UINT light_culling_id, UINT frame_index);
}
