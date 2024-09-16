#pragma once
#include "stdafx.h"
#include "Entity.h"
#include "FreeList.h"
#include "SharedTypes.h"

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
        utl::vector<d3d12::hlsl::DirectionalLightParameters> m_non_cullable_lights;
        utl::vector<UINT32> m_non_cullable_owners;
    };

    void generate_lights();
}