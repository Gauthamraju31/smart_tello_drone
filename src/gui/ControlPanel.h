#pragma once

#include "../core/TelloSDK.h"
#include <imgui.h>

class ControlPanel {
public:
    ControlPanel(TelloSDK& sdk);
    
    // Renders the flight controls inside ImGui
    void render(bool* p_open = nullptr);

private:
    void drawJoystick(const char* label, float* out_x, float* out_y, float key_x = 0.0f, float key_y = 0.0f);

    TelloSDK& m_sdk;
    int m_speed = 50;

    // Joystick states (-1.0 to 1.0)
    float m_leftJoyX = 0, m_leftJoyY = 0;   // Yaw, Throttle
    float m_rightJoyX = 0, m_rightJoyY = 0; // Roll, Pitch
    
    // Limits send rate to avoid flooding Tello (e.g. 20Hz max)
    int64_t m_lastRCSendTime = 0;
};
