#pragma once
#include "stdafx.h"
#include "Transform.h"
#include "Script.h"

namespace transform { struct init_info; }
namespace script { struct init_info; }

namespace game_entity {
    using namespace DirectX;
    
    constexpr UINT min_deleted_elements{ 4 };
    //constexpr UINT min_deleted_elements{ 1024 };

    struct entity_info
    {
        transform::init_info* transform{ nullptr };
        script::init_info* script{ nullptr };
    };

    class entity {
    public:
        constexpr entity(UINT id) : _id{ id } {}
        constexpr entity() : _id{ Invalid_Index } {}
        constexpr UINT get_id() const { return _id; }
        constexpr bool is_valid() const { return _id != Invalid_Index; }

        [[nodiscard]] transform::component get_transform() const;
        //[[nodiscard]] script::component get_script() const;

        [[nodiscard]] XMFLOAT4 rotation() const { return get_transform().rotation(); }
        [[nodiscard]] XMFLOAT3 orientation() const { return get_transform().orientation(); }
        [[nodiscard]] XMFLOAT3 position() const { return get_transform().position(); }
        [[nodiscard]] XMFLOAT3 scale() const { return get_transform().scale(); }

    private:
        UINT _id;
    };

    entity create(entity_info info);
    void remove(UINT id);
    bool is_alive(UINT id);

}
