#pragma once
#include "stdafx.h"
#include "Entity.h"

namespace app {

    void create_render_items();
    game_entity::entity create_entity_item(XMFLOAT3 position, XMFLOAT3 rotation, const char* script_name);
}

