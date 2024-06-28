#include "Transform.h"
#include "Entity.h"
#include "Vector.h"

namespace transform
{
    namespace {

        utl::vector<XMFLOAT4X4> to_world;
        utl::vector<XMFLOAT4X4> inv_world;
        utl::vector<XMFLOAT4> rotations;
        utl::vector<XMFLOAT3> orientations;
        utl::vector<XMFLOAT3> positions;
        utl::vector<XMFLOAT3> scales;
        utl::vector<UINT> has_transform;
        utl::vector<UINT> changes_from_previous_frame;
        UINT write_flag;

        XMFLOAT3 calculate_orientation(XMFLOAT4 rotation)
        {
            XMVECTOR rotation_quat{ XMLoadFloat4(&rotation) };
            XMVECTOR front{ XMVectorSet(0.f, 0.f, 1.f, 0.f) };
            XMFLOAT3 orientation;
            XMStoreFloat3(&orientation, XMVector3Rotate(front, rotation_quat));
            return orientation;
        }

    } // anonymous namespace

    component create(init_info info, game_entity::entity entity)
    {
        assert(entity.is_valid());
        const UINT entity_id{ entity.get_id() };

        if (positions.size() > entity_id)
        {
            // Reuse this id
            XMFLOAT4 rotation{ info.rotation };
            rotations[entity_id] = rotation;
            orientations[entity_id] = calculate_orientation(rotation);
            positions[entity_id] = XMFLOAT3{ info.position };
            scales[entity_id] = XMFLOAT3{ info.scale };
            has_transform[entity_id] = 0;
            changes_from_previous_frame[entity_id] = component_flags::all;
        }
        else
        {
            // Need to add a new entity
            assert(positions.size() == entity_id);

            to_world.emplace_back();
            inv_world.emplace_back();
            rotations.emplace_back(info.rotation);
            orientations.emplace_back(calculate_orientation(XMFLOAT4{ info.rotation }));
            positions.emplace_back(info.position);
            scales.emplace_back(info.scale);
            has_transform.emplace_back(0);
            changes_from_previous_frame.emplace_back(component_flags::all);
        }

        // NOTE: each entity has a transform component. Therefor, id's for transform components
        //       are exactly the same as entity ids.
        return component{ entity_id };
    }

    void remove(component c)
    {
        assert(c.is_valid());
    }

    XMFLOAT4 component::rotation() const
    {
        return rotations[_id];
    }

    XMFLOAT3 component::orientation() const
    {
        return orientations[_id];
    }

    XMFLOAT3 component::position() const
    {
        return positions[_id];
    }

    XMFLOAT3 component::scale() const
    {
        return scales[_id];
    }
}