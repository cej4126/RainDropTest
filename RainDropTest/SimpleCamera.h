#pragma once

using namespace DirectX;

class SimpleCamera
{
public:
    SimpleCamera();

    void Init(XMFLOAT3 position);
    void Update(float elapsed_seconds);
    XMMATRIX GetViewMatrix();
    XMMATRIX GetProjectionMatrix(float fov, float aspect_ratio, float near_plane = 1.0f, float far_plane = 1000.0f);
    void SetMoveSpeed(float units_per_second);
    void SetTurnSpeed(float radians_per_second);

    void OnKeyDown(WPARAM key);
    void OnKeyUp(WPARAM key);

private:
    void Reset();

    struct KeysPressed
    {
        bool w;
        bool a;
        bool s;
        bool d;

        bool left;
        bool right;
        bool up;
        bool down;
    };

    XMFLOAT3 m_initial_position;
    XMFLOAT3 m_position;
    float m_yaw;				// Relative to the +z axis.
    float m_pitch;				// Relative to the xz plane.
    XMFLOAT3 m_look_direction;
    XMFLOAT3 m_up_direction;
    float m_move_speed;         // Speed at which the camera moves, in units per second.
    float m_turn_speed;         // Speed at which the camera turns, in radians per second.

    KeysPressed m_key_pressed;

};

