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
    ZeroMemory(&m_key_pressed, sizeof(m_key_pressed));
}

void SimpleCamera::Init(XMFLOAT3 position)
{
    m_initial_position = position;
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
