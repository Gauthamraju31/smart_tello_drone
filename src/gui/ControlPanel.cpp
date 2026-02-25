#include "ControlPanel.h"
#include <cmath>
#include <chrono>

namespace {
    int64_t now_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
}

ControlPanel::ControlPanel(TelloSDK& sdk) : m_sdk(sdk) {}

void ControlPanel::drawJoystick(const char* label, float* out_x, float* out_y) {
    ImGui::Text("%s", label);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float radius = 60.0f;
    float cx = p.x + radius;
    float cy = p.y + radius;
    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    // Background circle
    draw_list->AddCircleFilled(ImVec2(cx, cy), radius, IM_COL32(50, 50, 50, 255));
    // Crosshairs
    draw_list->AddLine(ImVec2(cx - radius, cy), ImVec2(cx + radius, cy), IM_COL32(100, 100, 100, 255));
    draw_list->AddLine(ImVec2(cx, cy - radius), ImVec2(cx, cy + radius), IM_COL32(100, 100, 100, 255));

    // Calculate knob position based on input state
    float knob_x = cx + (*out_x * radius);
    float knob_y = cy - (*out_y * radius); // Negative because Y goes down in screen space

    // Handle mouse interaction
    ImGui::InvisibleButton((std::string("joy_btn_") + label).c_str(), ImVec2(radius * 2, radius * 2));
    
    bool active = ImGui::IsItemActive();
    if (active) {
        ImVec2 mouse_pos = ImGui::GetIO().MousePos;
        float dx = mouse_pos.x - cx;
        float dy = mouse_pos.y - cy;
        
        // Clamp to circle
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist > radius) {
            dx = (dx / dist) * radius;
            dy = (dy / dist) * radius;
        }
        
        *out_x = dx / radius;
        *out_y = -dy / radius; // Invert Y
        knob_x = cx + dx;
        knob_y = cy + dy;
    } else {
        // Auto-center when released
        *out_x = 0.0f;
        *out_y = 0.0f;
        knob_x = cx;
        knob_y = cy;
    }

    // Draw Knob
    ImU32 knob_color = active ? IM_COL32(200, 200, 200, 255) : IM_COL32(150, 150, 150, 255);
    draw_list->AddCircleFilled(ImVec2(knob_x, knob_y), radius * 0.3f, knob_color);
    
    ImGui::Text("\n X: %.2f  Y: %.2f", *out_x, *out_y);
}

void ControlPanel::render(bool* p_open) {
    if (ImGui::Begin("Flight Controls", p_open)) {
        
        // Large Emergency button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        if (ImGui::Button("EMERGENCY STOP (Esc)", ImVec2(-1, 50))) {
            m_sdk.emergency();
        }
        ImGui::PopStyleColor(3);

        ImGui::Separator();
        
        if (ImGui::Button("Takeoff (Space)", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 5, 40))) m_sdk.takeoff();
        ImGui::SameLine();
        if (ImGui::Button("Land (L)", ImVec2(-1, 40))) m_sdk.land();
        
        ImGui::Separator();
        
        // Speed slider
        if (ImGui::SliderInt("Speed %", &m_speed, 10, 100)) {
            m_sdk.setSpeed(m_speed);
        }

        ImGui::Separator();
        
        // Virtual Joysticks
        ImGui::BeginGroup();
        drawJoystick("Yaw / Throttle", &m_leftJoyX, &m_leftJoyY);
        ImGui::EndGroup();
        
        ImGui::SameLine(0, 50);
        
        ImGui::BeginGroup();
        drawJoystick("Roll / Pitch", &m_rightJoyX, &m_rightJoyY);
        ImGui::EndGroup();

        // Send RC command at 20Hz if any stick is not centered, or send 0 once
        int64_t now = now_ms();
        if (now - m_lastRCSendTime > 50) { // 50ms = 20Hz
            int roll = (int)(m_rightJoyX * 100.0f);
            int pitch = (int)(m_rightJoyY * 100.0f);
            int throttle = (int)(m_leftJoyY * 100.0f);
            int yaw = (int)(m_leftJoyX * 100.0f);
            
            if (roll != 0 || pitch != 0 || throttle != 0 || yaw != 0) {
                m_sdk.sendRC(roll, pitch, throttle, yaw);
                m_lastRCSendTime = now;
            } else if (m_lastRCSendTime != 0) {
                // Send zero once to stop
                m_sdk.sendRC(0, 0, 0, 0);
                m_lastRCSendTime = 0; // mark stopped
            }
        }
    }
    ImGui::End();
}
