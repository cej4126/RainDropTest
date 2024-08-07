#include "Camera.h"
#pragma once
#include "stdafx.h"
#include "FreeList.h"
#include "Entity.h"
#include "Transform.h"

namespace d3d12::camera {

    namespace {

        utl::free_list<Camera> cameras;

    } // anonymous namespace

    Camera::Camera(camera_init_info info)
        :
        m_entity_id{ info.entity_id },
        m_projection_type{ info.type },
        m_up{ XMLoadFloat3(&info.up) },
        m_field_of_view{ info.field_of_view },
        m_aspect_ratio{ info.aspect_ratio },
        m_near_z{ info.near_z },
        m_far_z{ info.far_z }
    {
        assert(m_entity_id != Invalid_Index);
        m_is_dirty = true;
        update();
    }

    void Camera::update()
    {
        game_entity::entity entity{ m_entity_id };
        XMFLOAT3 pos{ entity.transform().position() };
        XMFLOAT3 dir{ entity.transform().orientation() };
        m_position = XMLoadFloat3(&pos);
        m_direction = XMLoadFloat3(&dir);
        m_view = XMMatrixLookAtRH(m_position, m_direction, m_up);

        if (m_is_dirty)
        {
            if (m_projection_type == camera_type::perspective)
            {
                m_projection = XMMatrixPerspectiveFovRH(m_field_of_view * XM_PI, m_aspect_ratio, m_near_z, m_far_z);
            }
            else
            {
                m_projection = XMMatrixOrthographicRH(m_view_width, m_view_height, m_near_z, m_far_z);
            }
            m_inverse_projection = XMMatrixInverse(nullptr, m_projection);
            m_is_dirty = false;
        }

        m_view_projection = XMMatrixMultiply(m_view, m_projection);
        m_inverse_view_projection = XMMatrixInverse(nullptr, m_view_projection);
    }

    void mouse_move(input::input_source::type type, input::input_code::code code, const input::input_value& mouse_pos)
    {
    }

    void Camera::field_of_view(float fov)
    {
        assert(m_projection_type == camera::camera_type::perspective);
        m_field_of_view = fov;
        m_is_dirty = true;
    }

    void Camera::aspect_ratio(float aspect_ratio1)
    {
        assert(m_projection_type == camera::camera_type::perspective);
        m_aspect_ratio = aspect_ratio1;
        m_is_dirty = true;
    }

    void Camera::view_width(float width)
    {
        assert(m_projection_type == camera::camera_type::orthographic);
        m_view_width = width;
        m_is_dirty = true;
    }

    void Camera::view_height(float height)
    {
        assert(m_projection_type == camera::camera_type::orthographic);
        m_view_height = height;
        m_is_dirty = true;
    }

    void Camera::near_z(float near_z)
    {
        m_near_z = near_z;
        m_is_dirty = true;
    }

    void Camera::far_z(float far_z)
    {
        m_far_z = far_z;
        m_is_dirty = true;
    }

    UINT create(camera_init_info info)
    {
        return cameras.add(info);
    }

    void remove(UINT id)
    {
        assert(id != Invalid_Index);
        cameras.remove(id);
    }

    Camera& get(UINT id)
    {
        assert(id != Invalid_Index);
        return cameras[id];
    }


    void field_of_view(UINT id, float fov)
    {
        assert(id != Invalid_Index);
        cameras[id].field_of_view(fov);
    }

    void aspect_ratio(UINT id, float aspect_ratio)
    {
        assert(id != Invalid_Index);
        cameras[id].field_of_view(aspect_ratio);
    }

    void view_width(UINT id, float width)
    {
        assert(id != Invalid_Index);
        cameras[id].field_of_view(width);
    }

    void view_height(UINT id, float height)
    {
        assert(id != Invalid_Index);
        cameras[id].field_of_view(height);
    }

    void near_z(UINT id, float near_z)
    {
        assert(id != Invalid_Index);
        cameras[id].field_of_view(near_z);
    }

    void far_z(UINT id, float far_z)
    {
        assert(id != Invalid_Index);
        cameras[id].field_of_view(far_z);
    }
}

