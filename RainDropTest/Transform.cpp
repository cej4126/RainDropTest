#include "Transform.h"
#include "Entity.h"
#include "Vector.h"

namespace transform
{
    namespace {

        utl::vector<XMFLOAT4X4> to_worlds;
        utl::vector<XMFLOAT4X4> inverse_worlds;
        utl::vector<XMFLOAT4> rotations;
        utl::vector<XMFLOAT3> orientations;
        utl::vector<XMFLOAT3> positions;
        utl::vector<XMFLOAT3> scales;
        utl::vector<UINT> has_transform;
        utl::vector<UINT> changes_from_previous_frame;
        UINT write_flag;

        void calculate_transform_matrices(UINT id)
        {
            XMVECTOR r{ XMLoadFloat4(&rotations[id]) };
            XMVECTOR t{ XMLoadFloat3(&positions[id]) };
            XMVECTOR s{ XMLoadFloat3(&scales[id]) };

            XMMATRIX world{ XMMatrixAffineTransformation(s, XMQuaternionIdentity(), r, t) };
            XMStoreFloat4x4(&to_worlds[id], world);

            // NOTE: (F. Luna) Intro to DirectX 12, section 8.2.2
            // https://terrorgum.com/tfox/books/introductionto3dgameprogrammingwithdirectx12.pdf
            world.r[3] = XMVectorSet(0.f, 0.f, 0.f, 1.f);
            XMMATRIX inverse_world{ XMMatrixInverse(nullptr, world) };
            XMStoreFloat4x4(&inverse_worlds[id], inverse_world);

            has_transform[id] = 1;
        }

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

            to_worlds.emplace_back();
            inverse_worlds.emplace_back();
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


    void get_transform_matrices(UINT id, XMFLOAT4X4 world, XMFLOAT4X4 inverse_world)
    {
        assert(id != Invalid_Index);

        if (!has_transform[id])
        {
            calculate_transform_matrices(id);
        }

        world = to_worlds[id];
        inverse_world = inverse_worlds[id];
    }

    void update(const component_cache* const cache, UINT count)
    {}

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