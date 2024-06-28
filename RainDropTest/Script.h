#pragma once
#include "stdafx.h"
#include "Entity.h"

namespace script {

    struct init_info
    {
        UINT i;
    };

    class component final
    {
    public:
        constexpr component(UINT id) : _id{ id } {}
        constexpr component() : _id{ Invalid_Index } {}
        constexpr UINT get_id() const { return _id; }
        constexpr bool is_valid() const { return _id != Invalid_Index; }
    private:
        UINT _id;
    };

    component create(init_info info, game_entity::entity entity);
    void remove(component c);
    void update(float dt);
}

