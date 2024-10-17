#pragma once
#include "stdafx.h"
#include "Entity.h"
#include "Geometry.h"

namespace app {

    constexpr UINT min_deleted_elements{ 1024 };

    void create_render_items();
    game_entity::entity create_entity_item(XMFLOAT3 position, XMFLOAT3 rotation, XMFLOAT3 scale, geometry::init_info* geometry_info, const char* script_name);
}
