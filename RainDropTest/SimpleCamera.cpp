#include "stdafx.h"
#include "SimpleCamera.h"

SimpleCamera::SimpleCamera() :

    m_initial_position(0, 0, 0),
    m_position(m_initial_position),
    m_yaw(XM_PI),
    m_pitch(0.0f),
    m_look_direction(0, 0, -1),
    m_up_direction(0, 1, 0),
    m_move_speed(20.0f),
    m_turn_speed(XM_PIDIV2)
{
    ZeroMemory(&m_keys_pressed, sizeof(m_keys_pressed));
}

void SimpleCamera::Init(XMFLOAT3 position)
{
    m_initial_position = position;
    Reset();
}

void SimpleCamera::SetMoveSpeed(float units_per_second)
{
    m_move_speed = units_per_second;
}

void SimpleCamera::SetTurnSpeed(float radians_per_second)
{
    m_turn_speed = radians_per_second;
}

void SimpleCamera::Reset()
{
    m_position = m_initial_position;
    m_yaw = XM_PI;
    m_pitch = 0.0f;
    m_look_direction = { 0, 0, -1 };
}

void SimpleCamera::Update(float elapsed_seconds)
{
    // Calculate the move vector in camera space.
    XMFLOAT3 move(0, 0, 0);

    if (m_keys_pressed.a) move.x -= 1.0f;
    if (m_keys_pressed.d) move.x += 1.0f;
    if (m_keys_pressed.w) move.z -= 1.0f;
    if (m_keys_pressed.s) move.z += 1.0f;

    if (fabs(move.x) > 0.1f && fabs(move.z) > 0.1f)
    {
        XMVECTOR vector = XMVector3Normalize(XMLoadFloat3(&move));
        move.x = XMVectorGetX(vector);
        move.z = XMVectorGetZ(vector);
    }

    float move_interval = m_move_speed * elapsed_seconds;
    float rotate_interval = m_turn_speed * elapsed_seconds;

    if (m_keys_pressed.left) m_yaw += rotate_interval;
    if (m_keys_pressed.right) m_yaw += rotate_interval;
    if (m_keys_pressed.up) m_pitch += rotate_interval;
    if (m_keys_pressed.down) m_pitch += rotate_interval;

    // Prevent looking too far up or down.
    m_pitch = min(m_pitch, XM_PIDIV4);
    m_pitch = max(-XM_PIDIV4, m_pitch);

    // Move the camera in model space.
    float x = move.x * -cosf(m_yaw) - move.z * sinf(m_yaw);
    float z = move.x * sinf(m_yaw) - move.z * cosf(m_yaw);
    m_position.x += x * move_interval;
    m_position.z += z * move_interval;

    // Determine the look direction.
    float r = cosf(m_pitch);
    m_look_direction.x = r * sinf(m_yaw);
    m_look_direction.y = sinf(m_pitch);
    m_look_direction.z = r * cosf(m_yaw);
}

XMMATRIX SimpleCamera::GetViewMatrix()
{
    return XMMatrixLookToRH(XMLoadFloat3(&m_position), XMLoadFloat3(&m_look_direction), XMLoadFloat3(&m_up_direction));
}

XMMATRIX SimpleCamera::GetProjectionMatrix(float fov, float aspect_ratio, float near_plane, float far_plane)
{
    return XMMatrixPerspectiveFovRH(fov, aspect_ratio, near_plane, far_plane);
}

void SimpleCamera::OnKeyDown(WPARAM key)
{
    switch (key)
    {
    case 'W':
        m_keys_pressed.w = true;
        break;
    case 'A':
        m_keys_pressed.a = true;
        break;
    case 'S':
        m_keys_pressed.s = true;
        break;
    case 'D':
        m_keys_pressed.d = true;
        break;
    case VK_LEFT:
        m_keys_pressed.left = true;
        break;
    case VK_RIGHT:
        m_keys_pressed.right = true;
        break;
    case VK_UP:
        m_keys_pressed.up = true;
        break;
    case VK_DOWN:
        m_keys_pressed.down = true;
        break;
    case VK_ESCAPE:
        Reset();
        break;
    }
}


void SimpleCamera::OnKeyUp(WPARAM key)
{
    switch (key)
    {
    case 'W':
        m_keys_pressed.w = false;
        break;
    case 'A':
        m_keys_pressed.a = false;
        break;
    case 'S':
        m_keys_pressed.s = false;
        break;
    case 'D':
        m_keys_pressed.d = false;
        break;
    case VK_LEFT:
        m_keys_pressed.left = false;
        break;
    case VK_RIGHT:
        m_keys_pressed.right = false;
        break;
    case VK_UP:
        m_keys_pressed.up = false;
        break;
    case VK_DOWN:
        m_keys_pressed.down = false;
        break;
    }
}
