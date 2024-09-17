#pragma once
#include "stdafx.h"

namespace game_entity {
    class entity;
}

namespace geometry {

    class component final
    {
    public:
        constexpr explicit component(UINT id) : _id{ id } {}
        constexpr component() : _id{ Invalid_Index } {}
        constexpr UINT get_id() const { return _id; }
        constexpr bool is_valid() const { return _id != Invalid_Index; }
    private:
        UINT _id;
    };

    struct init_info
    {
        UINT geometry_content_id{ Invalid_Index };
        UINT material_count{ 0 };
        UINT* material_ids{ nullptr };
    };

    component create(init_info info, game_entity::entity entity);
    void remove(component c);
    void get_render_item_ids(UINT* const item_ids, UINT count);
}
