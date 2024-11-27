#include "Entity.h"
#include "Transform.h"
#include "Scripts.h"
#include "Geometry.h"
#include "Vector.h"
#include <deque>

namespace game_entity {
    namespace {
        utl::vector<transform::component> transforms;
        utl::vector<script::component> scripts;
        utl::vector<geometry::component> geometries;

        utl::vector<UINT> ids;
        std::deque<UINT> free_ids;
        bool min_deleted_set{ false };

    } // anonymous namespace

    XMFLOAT4 entity::rotation()
    {
        return transform().rotation();
    }

    XMFLOAT3 entity::orientation()
    {
        return transform().orientation();
    }

    XMFLOAT3 entity::position()
    {
        return transform().position();
    }

    XMFLOAT3 entity::scale()
    {
        return transform().scale();
    }

    entity create(entity_info info)
    {
        assert(info.transform);

        UINT id;

        if (free_ids.size() && (free_ids.size() > min_deleted_elements || min_deleted_set))
        {
            id = free_ids.front();
            free_ids.pop_front();
            ids[id] = id;
            min_deleted_set = true;
        }
        else
        {
            id = (UINT)ids.size();
            ids.push_back(id);

            // Resize components
            // NOTE: we don't call resize(), so the number of memory allocations stays low
            transforms.emplace_back();
            scripts.emplace_back();
            geometries.emplace_back();
        }

        const entity new_entity{ id };

        // Create transform component
        assert(!transforms[id].is_valid());
        transforms[id] = transform::create(*info.transform, new_entity);

        // create script component
        if (info.script && info.script->script_creator)
        {
            assert(!scripts[id].is_valid());
            scripts[id] = script::create(*info.script, new_entity);
            assert(scripts[id].is_valid());
        }

        // Create geometry component
        if (info.geometry)
        {
            assert(!geometries[id].is_valid());
            geometries[id] = geometry::create(*info.geometry, new_entity);
            assert(geometries[id].is_valid());
        }

        return new_entity;
    }

    void remove(UINT id)
    {
        assert(id != Invalid_Index);

        if (geometries[id].is_valid())
        {
            geometry::remove(geometries[id]);
            geometries[id] = {};
        }

        if (scripts[id].is_valid())
        {
            script::remove(scripts[id]);
            scripts[id] = {};
        }

        transform::remove(transforms[id]);
        transforms[id] = {};

        ids[id] = Invalid_Index;
        free_ids.push_back(id);
    }

    bool is_alive(UINT id)
    {
        assert(id < ids.size());
        return (ids[id] != Invalid_Index);
    }

    transform::component entity::transform() const
    {
        assert(is_alive(_id));
        return transforms[_id];
    }

    script::component entity::script() const
    {
        assert(is_alive(_id));
        return scripts[_id];
    }

    geometry::component entity::geometry() const
    {
        assert(is_alive(_id));
        return geometries[_id];
    }

}
