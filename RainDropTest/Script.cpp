#include "Script.h"
#include "Entity.h"
#include "Vector.h"
#include <deque>

namespace script {

    namespace {
        d3d12::utl::vector<UINT> entity_scripts;
        d3d12::utl::vector<UINT> id_mapping;

        d3d12::utl::vector<UINT> ids;
        std::deque<UINT> free_ids;

    } // anonymous namespace

    component create(init_info info, game_entity::entity entity)
    {
        assert(entity.is_valid());
        UINT id;


        if (free_ids.size() > game_entity::min_deleted_elements)
        {
            id = free_ids.front();
            free_ids.pop_front();
            ids[id] = id;
        }
        else
        {
            id = (UINT)ids.size();
            ids.push_back(id);
        }

        // NOTE: each entity has a transform component. Therefor, id's for transform components
        //       are exactly the same as entity ids.
        return component{ id };
    }

    void remove(component c)
    {}

    void update(float dt)
    {}
}

