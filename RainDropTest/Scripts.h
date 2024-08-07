#pragma once
#include "Input.h"
#include "Entity.h"

namespace script
{
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

    class entity_script : public game_entity::entity
    {
    public:
        virtual ~entity_script() = default;
        virtual void begin_play() {}
        virtual void update(float) {}
    protected:
        constexpr explicit entity_script(game_entity::entity entity)
            : game_entity::entity{ entity.get_id() } {}

        void set_rotation(XMFLOAT4 rotation_quaternion) const { set_rotation(this, rotation_quaternion); }
        void set_orientation(XMFLOAT3 orientation_vector) const { set_orientation(this, orientation_vector); }
        void set_position(XMFLOAT3 position) const { set_position(this, position); }
        void set_scale(XMFLOAT3 scale) const { set_scale(this, scale); }

        static void set_rotation(const game_entity::entity* const entity, XMFLOAT4 rotation_quaternion);
        static void set_orientation(const game_entity::entity* const entity, XMFLOAT3 orientation_vector);
        static void set_position(const game_entity::entity* const entity, XMFLOAT3 position);
        static void set_scale(const game_entity::entity* const entity, XMFLOAT3 scale);
    };

    namespace detail {
        using script_ptr = std::unique_ptr<entity_script>;
        using script_creator = script_ptr(*)(game_entity::entity entity);

        UINT8 register_script(size_t, script_creator);
        script_creator get_script_creator(size_t tap);

        template<class script_class>
        script_ptr create_script(game_entity::entity entity)
        {
            assert(entity.is_valid());
            return std::make_unique<script_class>(entity);
        }
    }

    class camera_script : public script::entity_script
    {
    public:
        camera_script() = default;
        ~camera_script() = default;
        explicit camera_script(game_entity::entity entity);
        void begin_play() override {}
        void update(float dt) override;

    private:

        void on_move(UINT64 binding, const input::input_value& value);
        void mouse_move(input::input_source::type type, input::input_code::code code, const input::input_value& mouse_pos);
        void camera_seek(float dt);

        input::input_system<camera_script>  _input_system{};

        DirectX::XMVECTOR                   _desired_position;
        DirectX::XMVECTOR                   _desired_spherical;
        DirectX::XMVECTOR                   _position;
        DirectX::XMVECTOR                   _spherical;
        DirectX::XMVECTOR                   _move{};
        float                               _move_magnitude{ 0.f };
        float                               _position_acceleration{ 0.f };
        bool                                _move_position{ false };
        bool                                _move_rotation{ false };
    };

    struct init_info
    {
        detail::script_creator script_creator;
    };

    component create(init_info info, game_entity::entity entity);
    void remove(component c);
    void update(float dt);

#define REGISTER_SCRIPT(TYPE) \
    namespace { \
       const UINT8 _reg_##TYPE { \
           script::detail::register_script( std::hash<std::string>()(#TYPE), script::detail::create_script<TYPE>) }; \
    }
}
