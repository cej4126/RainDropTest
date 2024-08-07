#include <unordered_map>
#include "Scripts.h"
#include "Math.h"
#include "Entity.h"
#include "Input.h"
#include "Transform.h"
#include <deque>

#define USE_TRANSFORM_CACHE_MAP 0

namespace script {

    namespace {

        utl::vector<detail::script_ptr> entity_scripts;
        utl::vector<UINT> id_mapping;

        utl::vector<UINT> ids;
        std::deque<UINT> free_ids;

        utl::vector<transform::component_cache> transform_cache;
#if USE_TRANSFORM_CACHE_MAP
        std::unordered_map<UINT, UINT>    cache_map;
#endif

        using script_registry = std::unordered_map<size_t, detail::script_creator>;
        script_registry& registry()
        {
            // NOTE: we put this static variable in a function because of
            //       the initialization order of static data. This way, we can
            //       be certain that the data is initialized before accessing it.
            static script_registry reg;
            return reg;
        }

        bool exists(UINT id)
        {
            assert(id != Invalid_Index);
            assert(id_mapping[id] < entity_scripts.size());
            return (id_mapping[id] < entity_scripts.size());
        }

#if USE_TRANSFORM_CACHE_MAP
        transform::component_cache* const get_chage_ptr(const game_entity::entity* const entity)
        {
            assert(game_entity::is_alive((*entity).get_id()));
            const UINT id{ (*entity).transform().get_id() };

            UINT index{ Invalid_Index };
            auto pair = cache_map.try_emplace(id, Invalid_Index);

            // cache_map didn't have an entry for this id, new entry inserted
            if (pair.second)
            {
                index = (UINT)transform_cache.size();
                transform_cache.emplace_back();
                transform_cache.back().id = id;
                cache_map[id] = index;
            }
            else
            {
                index = cache_map[id];
            }

            assert(index < transform_cache.size());
            return &transform_cache[index];
        }
#else
        transform::component_cache* const get_cache_ptr(const game_entity::entity* const entity)
        {
            assert(game_entity::is_alive((*entity).get_id()));
            const UINT id{ (*entity).transform().get_id() };

            for (auto& cache : transform_cache)
            {
                if (cache.id == id)
                {
                    return &cache;
                }
            }

            transform_cache.emplace_back();
            transform_cache.back().id = id;

            return &transform_cache.back();
        }

#endif

    } // anonymous namespace

    namespace detail {
        UINT8 register_script(size_t tag, script_creator func)
        {
            bool result{ registry().insert(script_registry::value_type{tag, func}).second };
            assert(result);
            return result;
        }

        script_creator get_script_creator(size_t tag)
        {
            auto script = registry().find(tag);
            assert(script != registry().end() && script->first == tag);
            return script->second;
        }
    }

    class camera_script;
    REGISTER_SCRIPT(camera_script);
    camera_script::camera_script(game_entity::entity entity)
        : script::entity_script{ entity }
    {
        _input_system.add_handler(input::input_source::mouse, this, &camera_script::mouse_move);

        const UINT64 binding{ std::hash<std::string>()("move") };
        _input_system.add_handler(binding, this, &camera_script::on_move);

        XMFLOAT3 pos{ position() };
        _desired_position = _position = DirectX::XMLoadFloat3(&pos);

        XMFLOAT3 dir{ orientation() };
        float theta{ DirectX::XMScalarACos(dir.y) };
        float phi{ std::atan2(-dir.z, dir.x) };
        XMFLOAT3 rot{ theta - math::half_pi, phi + math::half_pi, 0.f };
        _desired_spherical = _spherical = DirectX::XMLoadFloat3(&rot);
    }

    void camera_script::update(float dt)
    {
        using namespace DirectX;

        if (_move_magnitude > math::epsilon)
        {
            const float fps_scale{ dt / 0.016667f };
            XMFLOAT4 rot{ rotation() };
            XMVECTOR d{ XMVector3Rotate(_move * 0.05f * fps_scale, XMLoadFloat4(&rot)) };
            if (_position_acceleration < 1.f) _position_acceleration += (0.02f * fps_scale);
            _desired_position += (d * _position_acceleration);
            _move_position = true;
        }
        else if (_move_position)
        {
            _position_acceleration = 0.f;
        }

        if (_move_position || _move_rotation)
        {
            camera_seek(dt);
        }
    }

    void camera_script::on_move(UINT64 binding, const input::input_value& value)
    {
        using namespace DirectX;

        _move = XMLoadFloat3(&value.current);
        _move_magnitude = XMVectorGetX(XMVector3LengthSq(_move));
    }

    void camera_script::mouse_move(input::input_source::type type, input::input_code::code code, const input::input_value& mouse_pos)
    {
        using namespace DirectX;

        if (code == input::input_code::mouse_position)
        {
            input::input_value value;
            input::get(input::input_source::mouse, input::input_code::mouse_left, value);
            if (value.current.z == 0.f) return;

            const float scale{ 0.005f };
            const float dx{ (mouse_pos.current.x - mouse_pos.previous.x) * scale };
            const float dy{ (mouse_pos.current.y - mouse_pos.previous.y) * scale };

            XMFLOAT3 spherical;
            DirectX::XMStoreFloat3(&spherical, _desired_spherical);
            spherical.x += dy;
            spherical.y -= dx;
            spherical.x = math::clamp(spherical.x, 0.0001f - math::half_pi, math::half_pi - 0.0001f);

            _desired_spherical = DirectX::XMLoadFloat3(&spherical);
            _move_rotation = true;
        }
    }

    void camera_script::camera_seek(float dt)
    {
        using namespace DirectX;
        XMVECTOR p{ _desired_position - _position };
        XMVECTOR o{ _desired_spherical - _spherical };

        _move_position = (XMVectorGetX(XMVector3LengthSq(p)) > math::epsilon);
        _move_rotation = (XMVectorGetX(XMVector3LengthSq(o)) > math::epsilon);

        const float scale{ 0.2f * dt / 0.016667f };

        if (_move_position)
        {
            _position += (p * scale);
            XMFLOAT3 new_pos;
            XMStoreFloat3(&new_pos, _position);
            set_position(new_pos);
        }

        if (_move_rotation)
        {
            _spherical += (o * scale);
            XMFLOAT3 new_rot;
            XMStoreFloat3(&new_rot, _spherical);
            new_rot.x = math::clamp(new_rot.x, 0.0001f - math::half_pi, math::half_pi - 0.0001f);
            _spherical = DirectX::XMLoadFloat3(&new_rot);

            DirectX::XMVECTOR quat{ DirectX::XMQuaternionRotationRollPitchYawFromVector(_spherical) };
            XMFLOAT4 rot_quat;
            DirectX::XMStoreFloat4(&rot_quat, quat);
            set_rotation(rot_quat);
        }
    }

    void entity_script::set_rotation(const game_entity::entity* const entity, XMFLOAT4 rotation_quaternion)
    {
        transform::component_cache& cache{ *get_cache_ptr(entity) };
        cache.flags |= transform::component_flags::rotation;
        cache.rotation = rotation_quaternion;
    }

    void entity_script::set_orientation(const game_entity::entity* const entity, XMFLOAT3 orientation_vector)
    {
        transform::component_cache& cache{ *get_cache_ptr(entity) };
        cache.flags |= transform::component_flags::orientation;
        cache.orientation = orientation_vector;
    }

    void entity_script::set_position(const game_entity::entity* const entity, XMFLOAT3 position)
    {
        transform::component_cache& cache{ *get_cache_ptr(entity) };
        cache.flags |= transform::component_flags::position;
        cache.position = position;
    }

    void entity_script::set_scale(const game_entity::entity* const entity, XMFLOAT3 scale)
    {
        transform::component_cache& cache{ *get_cache_ptr(entity) };
        cache.flags |= transform::component_flags::scale;
        cache.scale = scale;
    }

    component create(init_info info, game_entity::entity entity)
    {
        assert(entity.is_valid());
        assert(info.script_creator);
        UINT id;

        if (free_ids.size() > game_entity::min_deleted_elements)
        {
            id = free_ids.front();
            assert(!exists(id));
            free_ids.pop_front();
            ids[id] = id;
        }
        else
        {
            id = (UINT)ids.size();
            id_mapping.emplace_back();
            ids.push_back(id);
        }

        entity_scripts.emplace_back(info.script_creator(entity));
        id_mapping[id] = id;

        // NOTE: each entity has a transform component. Therefor, id's for transform components
        //       are exactly the same as entity ids.
        return component{ id };
    }

    void remove(component c)
    {
        assert(c.is_valid() && exists(c.get_id()));
        const UINT id{ c.get_id() };
        const UINT index{ id_mapping[id] };
        const UINT last_id{ entity_scripts.back()->script().get_id() };
        entity_scripts.erase_unordered(index);
        id_mapping[last_id] = index;
        id_mapping[id] = Invalid_Index;
    }

    void update(float dt)
    {
        for (auto& ptr : entity_scripts)
        {
            ptr->update(dt);
        }

        if (transform_cache.size())
        {
            transform::update(transform_cache.data(), (UINT)transform_cache.size());
            transform_cache.clear();
#if USE_TRANSFORM_CACHE_MAP
            cache_map.clear();
#endif
        }
    }
}