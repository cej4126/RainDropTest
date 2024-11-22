#include "Transform.h"
#include "Entity.h"
#include "Vector.h"

namespace transform
{
    namespace {

        utl::vector<XMFLOAT4X4> m_to_worlds;
        utl::vector<XMFLOAT4X4> m_inverse_worlds;
        utl::vector<XMFLOAT4> m_rotations;
        utl::vector<XMFLOAT3> m_orientations;
        utl::vector<XMFLOAT3> m_positions;
        utl::vector<XMFLOAT3> m_scales;
        utl::vector<UINT> m_has_transform;
        utl::vector<UINT> m_changes_from_previous_frame;
        UINT m_write_flag;

        void calculate_transform_matrices(UINT id)
        {
            XMVECTOR r{ XMLoadFloat4(&m_rotations[id]) };
            XMVECTOR t{ XMLoadFloat3(&m_positions[id]) };
            XMVECTOR s{ XMLoadFloat3(&m_scales[id]) };

            XMMATRIX world{ XMMatrixAffineTransformation(s, XMQuaternionIdentity(), r, t) };
            XMStoreFloat4x4(&m_to_worlds[id], world);

            // NOTE: (F. Luna) Intro to DirectX 12, section 8.2.2
            // https://terrorgum.com/tfox/books/introductionto3dgameprogrammingwithdirectx12.pdf
            world.r[3] = XMVectorSet(0.f, 0.f, 0.f, 1.f);
            XMMATRIX inverse_world{ XMMatrixInverse(nullptr, world) };
            XMStoreFloat4x4(&m_inverse_worlds[id], inverse_world);

            m_has_transform[id] = 1;
        }

        XMFLOAT3 calculate_orientation(XMFLOAT4 rotation)
        {
            XMVECTOR rotation_quat{ XMLoadFloat4(&rotation) };
            XMVECTOR front{ XMVectorSet(0.f, 0.f, 1.f, 0.f) };
            XMFLOAT3 orientation;
            XMStoreFloat3(&orientation, XMVector3Normalize(XMVector3Rotate(front, rotation_quat)));
            return orientation;
        }

        void set_rotation(UINT id, const XMFLOAT4& rotation_quaternion)
        {
            m_rotations[id] = rotation_quaternion;
            m_orientations[id] = calculate_orientation(rotation_quaternion);
            m_has_transform[id] = 0;
            m_changes_from_previous_frame[id] |= component_flags::rotation;
        }

        void set_orientation(UINT id, const XMFLOAT3& orientation)
        {
            m_orientations[id] = orientation;
            m_has_transform[id] = 0;
            m_changes_from_previous_frame[id] |= component_flags::orientation;
        }

        void set_position(UINT id, const XMFLOAT3& position)
        {
            m_positions[id] = position;
            m_has_transform[id] = 0;
            m_changes_from_previous_frame[id] |= component_flags::position;
        }

        void set_scale(UINT id, const XMFLOAT3& scale)
        {
            m_scales[id] = scale;
            m_has_transform[id] = 0;
            m_changes_from_previous_frame[id] |= component_flags::scale;
        }

    } // anonymous namespace

    component create(init_info info, game_entity::entity entity)
    {
        assert(entity.is_valid());
        const UINT entity_id{ entity.get_id() };

        if (m_positions.size() > entity_id)
        {
            // Reuse this id
            XMFLOAT4 rotation{ info.rotation };
            m_rotations[entity_id] = rotation;
            m_orientations[entity_id] = calculate_orientation(rotation);
            m_positions[entity_id] = XMFLOAT3{ info.position };
            m_scales[entity_id] = XMFLOAT3{ info.scale };
            m_has_transform[entity_id] = 0;
            m_changes_from_previous_frame[entity_id] = component_flags::all;
        }
        else
        {
            // Need to add a new entity
            assert(m_positions.size() == entity_id);

            m_to_worlds.emplace_back();
            m_inverse_worlds.emplace_back();
            m_rotations.emplace_back(info.rotation);
            m_orientations.emplace_back(calculate_orientation(XMFLOAT4{ info.rotation }));
            m_positions.emplace_back(info.position);
            m_scales.emplace_back(info.scale);
            m_has_transform.emplace_back(0);
            m_changes_from_previous_frame.emplace_back(component_flags::all);
        }

        // NOTE: each entity has a transform component. Therefor, id's for transform components
        //       are exactly the same as entity ids.
        return component{ entity_id };
    }

    void remove(component c)
    {
        assert(c.is_valid());
    }

    void get_transform_matrices(UINT id, XMFLOAT4X4& world, XMFLOAT4X4& inverse_world)
    {
        assert(id != Invalid_Index);

        if (!m_has_transform[id])
        {
            calculate_transform_matrices(id);
        }

        world = m_to_worlds[id];
        inverse_world = m_inverse_worlds[id];
    }

    void get_updated_components_flags(const UINT* const ids, UINT count, UINT8* const flags)
    {
        assert(ids && count && flags);
        m_write_flag = 1;

        for (UINT i{ 0 }; i < count; ++i)
        {
            assert(ids[i] != Invalid_Index);
            flags[i] = m_changes_from_previous_frame[ids[i]];
        }
    }

    void update(const component_cache* const cache, UINT count)
    {
        assert(cache && count);

        // NOTE: clearing "changes_from_previous_frame" happens once every frame when there will be no reads and the caches are
        //       about to be applied by calling this function (i.e. the rest of the current frame will only have writes).
        if (m_write_flag)
        {
            memset(m_changes_from_previous_frame.data(), 0, m_changes_from_previous_frame.size());
            m_write_flag = 0;
        }

        for (UINT i{ 0 }; i < count; ++i)
        {
            const component_cache& c{ cache[i] };
            assert(component{ c.id }.is_valid());

            if (c.flags & component_flags::rotation)
            {
                set_rotation(c.id, c.rotation);
            }

            if (c.flags & component_flags::orientation)
            {
                set_orientation(c.id, c.orientation);
            }

            if (c.flags & component_flags::position)
            {
                set_position(c.id, c.position);
            }

            if (c.flags & component_flags::scale)
            {
                set_scale(c.id, c.scale);
            }
        }
    }

    XMFLOAT4 component::rotation() const
    {
        return m_rotations[_id];
    }

    XMFLOAT3 component::orientation() const
    {
        return m_orientations[_id];
    }

    XMFLOAT3 component::position() const
    {
        return m_positions[_id];
    }

    XMFLOAT3 component::scale() const
    {
        return m_scales[_id];
    }
}