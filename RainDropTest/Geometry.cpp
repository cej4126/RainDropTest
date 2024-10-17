#include "stdafx.h"
#include "Geometry.h"
#include "Entity.h"
#include "Vector.h"
#include "AppItems.h"
#include "Content.h"
#include <deque>

namespace geometry {
    namespace {
        utl::vector<UINT> active_lod;
        utl::vector<UINT> geometry_item_ids;
        utl::vector<UINT> owner_ids;
        utl::vector<UINT> id_mapping;

        //utl::vector<UINT> generations;
        std::deque<UINT> free_ids;

    } // anonymous namespace

    component create(init_info info, game_entity::entity entity)
    {
        assert(entity.get_id() != Invalid_Index);
        assert(info.geometry_content_id != Invalid_Index && info.material_count && info.material_ids);

        UINT id{};
        if (free_ids.size() > app::min_deleted_elements)
        {
            id = free_ids.front();
            free_ids.pop_front();
        }
        else
        {
            id = (UINT)id_mapping.size();
            id_mapping.emplace_back();
        }

        assert(id != Invalid_Index);
        UINT index{ (UINT)geometry_item_ids.size() };
        active_lod.emplace_back(0);
        geometry_item_ids.emplace_back(content::render_item::add(entity.get_id(), info.geometry_content_id, info.material_count, info.material_ids));
        owner_ids.emplace_back(id);
        id_mapping[id] = index;
        return component{ id };
    }

    void remove(component c)
    {
        assert(c.is_valid());
        const UINT id{ c.get_id() };
        const UINT index{ id_mapping[id] };
        const UINT last_id{ owner_ids.back() };
        content::render_item::remove(geometry_item_ids[index]);
        active_lod.erase_unordered(index);
        geometry_item_ids.erase_unordered(index);
        owner_ids.erase_unordered(index);
        id_mapping[last_id] = index;
        id_mapping[id] = Invalid_Index;
        free_ids.push_back(id);
    }

    void get_geometry_item_ids(UINT* const item_ids, UINT count)
    {
        assert(geometry_item_ids.size() >= count);
        memcpy(item_ids, geometry_item_ids.data(), count * sizeof(UINT));
    }
}
