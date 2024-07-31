#pragma once
#include "stdafx.h"


namespace game_entity{
    class entity;
}

namespace transform {

    struct init_info
    {
        float position[3]{};
        float rotation[4]{};
        float scale[3]{ 1.f, 1.f, 1.f };
    };

    struct component_flags {
        enum flags : UINT {
            rotation = 0x01,
            orientation = 0x02,
            position = 0x04,
            scale = 0x08,

            all = rotation | orientation | position | scale
        };
    };

    struct component_cache
    {
        XMFLOAT4 rotation;
        XMFLOAT3 orientation;
        XMFLOAT3 position;
        XMFLOAT3 scale;
        UINT id;
        UINT flags;
    };

    class component final
    {
    public:
        constexpr component(UINT id) : _id{ id } {}
        constexpr component() : _id{ Invalid_Index } {}
        constexpr UINT get_id() const { return _id; }
        constexpr bool is_valid() const { return _id != Invalid_Index; }

        XMFLOAT4 rotation() const;
        XMFLOAT3 orientation() const;
        XMFLOAT3 position() const;
        XMFLOAT3 scale() const;

    private:
        UINT _id;
    };

    component create(init_info info, game_entity::entity entity);
    void remove(component c);

    void get_transform_matrices(UINT id, XMFLOAT4X4 world, XMFLOAT4X4 inverse_world);
    void update(const component_cache* const cache, UINT count);

}
