#pragma once
#include "stdafx.h"
#include "Input.h"

namespace d3d12::camera {

    struct camera_type {
        enum type : UINT {
            perspective,
            orthographic
        };
    };

    struct camera_init_info
    {
        UINT entity_id{ Invalid_Index };
        camera_type::type type{};
        XMFLOAT3 up{};
        union {
            float field_of_view{ 0.f };
            float view_width;
        };
        union {
            float aspect_ratio;
            float view_height{ 0.f };
        };
        float near_z{ 0.f };
        float far_z{ 0.f };
    };

    struct perspective_camera_init_info : public camera_init_info
    {
        explicit perspective_camera_init_info(UINT id)
        {
            assert(id != Invalid_Index);
            entity_id = id;
            type = camera_type::perspective;
            up = { 0.f, 1.f, 0.f };
            field_of_view = 0.25f;
            aspect_ratio = 16.f / 10.f;
            near_z = 0.01f;
            far_z = 10000.f;
        }
    };

    struct orthographic_camera_init_info : public camera_init_info
    {
        explicit orthographic_camera_init_info(UINT id)
        {
            assert(id != Invalid_Index);
            entity_id = id;
            type = camera::camera_type::orthographic;
            up = { 0.f, 1.f, 0.f };
            view_width = 1920;
            view_height = 1000;
            near_z = 0.01f;
            far_z = 10000.f;
        }
    };

    class Camera
    {
    public:
        //Camera() = default;
        //explicit Camera(UINT id) : _id{ id } {}
        explicit Camera(camera_init_info info);
        void update();

        void field_of_view(float fov);
        void aspect_ratio(float aspect_ratio);
        void view_width(float width);
        void view_height(float height);
        void near_z(float near_z);
        void far_z(float far_z);

        [[nodiscard]] constexpr DirectX::XMMATRIX view() const { return m_view; }
        [[nodiscard]] constexpr DirectX::XMMATRIX inverse_view() const { return m_inverse_view; }
        [[nodiscard]] constexpr DirectX::XMMATRIX projection() const { return m_projection; }
        [[nodiscard]] constexpr DirectX::XMMATRIX inverse_projection() const { return m_inverse_projection; }
        [[nodiscard]] constexpr DirectX::XMMATRIX view_projection() const { return m_view_projection; }
        [[nodiscard]] constexpr DirectX::XMMATRIX inverse_view_projection() const { return m_inverse_view_projection; }
        [[nodiscard]] constexpr DirectX::XMVECTOR position() const { return m_position; }
        [[nodiscard]] constexpr DirectX::XMVECTOR direction() const { return m_direction; }
        [[nodiscard]] constexpr DirectX::XMVECTOR up() const { return m_up; }
        [[nodiscard]] constexpr float near_z() const { return m_near_z; }
        [[nodiscard]] constexpr float far_z() const { return m_far_z; }
        [[nodiscard]] constexpr float field_of_view() const { return m_field_of_view; }
        [[nodiscard]] constexpr float aspect_ratio() const { return m_aspect_ratio; }
        [[nodiscard]] constexpr float view_width() const { return m_view_width; }
        [[nodiscard]] constexpr float view_height() const { return m_view_height; }
        [[nodiscard]] constexpr camera_type::type projection_type() const { return m_projection_type; }
        [[nodiscard]] constexpr UINT entity_id() const { return m_entity_id; }

    private:
        XMMATRIX m_view{};
        XMMATRIX m_inverse_view{};
        XMMATRIX m_projection{};
        XMMATRIX m_inverse_projection{};
        XMMATRIX m_view_projection{};
        XMMATRIX m_inverse_view_projection{};
        XMVECTOR m_position{};
        XMVECTOR m_direction{};
        XMVECTOR m_up{};
        float m_near_z{ 0.f };
        float m_far_z{ 0.f };
        union {
            float m_field_of_view{ 0.f };
            float m_view_width;
        };
        union {
            float m_aspect_ratio{ 0.f };
            float m_view_height;
        };
        camera_type::type m_projection_type{};
        UINT m_entity_id{ Invalid_Index };
        bool m_is_dirty{ false };

        UINT _id{ Invalid_Index };
        //void mouse_move(input::input_source::type type, input::input_code::code code, const input::input_value& mouse_pos);
        input::input_system<Camera> m_input_system;
    };

    UINT create(camera_init_info info);
    void remove(UINT id);
    [[nodiscard]] Camera& get(UINT id);

    void field_of_view(UINT id, float fov);
    void aspect_ratio(UINT id, float aspect_ratio);
    void view_width(UINT id, float width);
    void view_height(UINT id, float height);
    void near_z(UINT id, float near_z);
    void far_z(UINT id, float far_z);

}

