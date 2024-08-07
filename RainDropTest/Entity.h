#pragma once
#include "stdafx.h"

namespace transform {
    class component;
}

namespace script {
    class component;
}

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

        [[nodiscard]] transform::component transform() const;
        [[nodiscard]] script::component script() const;

        [[nodiscard]] XMFLOAT4 rotation();
        [[nodiscard]] XMFLOAT3 orientation();
        [[nodiscard]] XMFLOAT3 position();
        [[nodiscard]] XMFLOAT3 scale();

    private:
        UINT _id;
    };

    entity create(entity_info info);
    void remove(UINT id);
    bool is_alive(UINT id);
} // namespace game_entity
