#include "SettingsPanel.h"

SettingsPanel::SettingsPanel() {}

void SettingsPanel::render(bool* p_open) {
    if (ImGui::Begin("Settings", p_open)) {
        
        if (ImGui::CollapsingHeader("Drone Configuration", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::InputText("WiFi SSID", m_ssid, sizeof(m_ssid));
            ImGui::InputText("WiFi Password", m_password, sizeof(m_password), ImGuiInputTextFlags_Password);
            if (ImGui::Button("Update Credentials (Connects Tello to AP)")) {
                // Future Implementation
            }
            
            ImGui::Separator();
            
            ImGui::Checkbox("Enable Mission Pad Detection", &m_missionPadEnabled);
            
            const char* bitrateOptions[] = {"Auto", "1 Mbps", "2 Mbps", "3 Mbps", "4 Mbps", "5 Mbps"};
            if (ImGui::Combo("Video Bitrate", &m_videoBitrate, bitrateOptions, IM_ARRAYSIZE(bitrateOptions))) {
                // Future Implementation: m_sdk.sendCommand("setbitrate ...");
            }
        }
        
        if (ImGui::CollapsingHeader("AI / Processing (Placeholders)", ImGuiTreeNodeFlags_DefaultOpen)) {
            // These would normally toggle states in the FrameProcessors and SLAMEngine
            ImGui::Checkbox("Object Tracking (YOLOv8)", &m_aiTracking);
            ImGui::Checkbox("Instance Segmentation (SAM)", &m_aiSegmentation);
            ImGui::Separator();
            ImGui::Checkbox("Monocular SLAM (Map Viewer)", &m_slamEnabled);
            
            if (m_aiTracking || m_aiSegmentation || m_slamEnabled) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Warning: AI placeholders consume CPU");
            }
        }
    }
    ImGui::End();
}
