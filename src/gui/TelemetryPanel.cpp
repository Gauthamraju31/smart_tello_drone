#include "TelemetryPanel.h"
#include <cmath>

TelemetryPanel::TelemetryPanel(TelemetryLogger& logger) : m_logger(logger) {}

void TelemetryPanel::renderBatteryBar(int bat) {
    ImVec4 color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // Green
    if (bat < 20) color = ImVec4(0.8f, 0.2f, 0.2f, 1.0f); // Red
    else if (bat < 40) color = ImVec4(0.8f, 0.8f, 0.2f, 1.0f); // Yellow

    ImGui::Text("Battery: %d%%", bat);
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    ImGui::ProgressBar(bat / 100.0f, ImVec2(-1.0f, 15.0f), "");
    ImGui::PopStyleColor();
}

void TelemetryPanel::renderAttitudeDial(int pitch, int roll, int yaw) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    
    float radius = 50.0f;
    float cx = p.x + radius;
    float cy = p.y + radius;

    // Draw background
    draw_list->AddCircleFilled(ImVec2(cx, cy), radius, IM_COL32(50, 50, 50, 255));
    
    // Artificial horizon implementation using ImDrawList
    // (A simplified version: a line that rolls and shifts with pitch)
    float rRad = roll * 3.14159f / 180.0f;
    float pShift = (pitch / 90.0f) * radius;

    float dx = cos(rRad) * radius * 0.8f;
    float dy = sin(rRad) * radius * 0.8f;

    // Horizon line
    draw_list->AddLine(
        ImVec2(cx - dx - sin(rRad)*pShift, cy + dy - cos(rRad)*pShift),
        ImVec2(cx + dx - sin(rRad)*pShift, cy - dy - cos(rRad)*pShift),
        IM_COL32(255, 255, 255, 255), 2.0f
    );

    // Crosshair (Drone)
    draw_list->AddLine(ImVec2(cx - 10, cy), ImVec2(cx + 10, cy), IM_COL32(255, 0, 0, 255), 2.0f);
    draw_list->AddLine(ImVec2(cx, cy - 10), ImVec2(cx, cy + 10), IM_COL32(255, 0, 0, 255), 2.0f);

    // Draw circle outline
    draw_list->AddCircle(ImVec2(cx, cy), radius, IM_COL32(150, 150, 150, 255), 32, 2.0f);

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + radius * 2 + 10));
    ImGui::Text("P:%4d  R:%4d  Y:%4d", pitch, roll, yaw);
}

void TelemetryPanel::renderAltitudeSpeed(int h, float baro, int vgx, int vgy, int vgz) {
    ImGui::Text("Altitude (ToF): %d cm", h);
    ImGui::Text("Altitude (Baro): %.1f m", baro);
    ImGui::Separator();
    
    float speed = std::sqrt(vgx*vgx + vgy*vgy + vgz*vgz);
    ImGui::Text("Speed: %.1f cm/s", speed);
    ImGui::Text(" Vx: %4d | Vy: %4d | Vz: %4d", vgx, vgy, vgz);
}

void TelemetryPanel::render(bool* p_open) {
    if (ImGui::Begin("Telemetry", p_open)) {
        TelemetryData t = m_logger.getLatest();

        renderBatteryBar(t.bat);
        ImGui::Separator();
        
        ImGui::BeginGroup();
        renderAttitudeDial(t.pitch, t.roll, t.yaw);
        ImGui::EndGroup();
        
        ImGui::SameLine(0, 30);
        
        ImGui::BeginGroup();
        renderAltitudeSpeed(t.h, t.baro, t.vgx, t.vgy, t.vgz);
        ImGui::Separator();
        ImGui::Text("Temp: %d-%d C", t.templ, t.temph);
        ImGui::Text("Time: %d s", t.flightTime);
        ImGui::EndGroup();
    }
    ImGui::End();
}
