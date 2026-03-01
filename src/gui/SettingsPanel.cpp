#include "SettingsPanel.h"
#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>
#include <cstring>

SettingsPanel::SettingsPanel(SLAMEngine& slamEngine, std::shared_ptr<Segmentation> segmentation) 
    : m_slamEngine(slamEngine), m_segmentation(segmentation) {
    m_slamEnabled = m_slamEngine.isEnabled();
    
    try {
        YAML::Node config = YAML::LoadFile("config/settings.yaml");
        if (config["drone"]) {
            if (config["drone"]["ssid"]) {
                std::string ssid = config["drone"]["ssid"].as<std::string>();
                std::strncpy(m_ssid, ssid.c_str(), sizeof(m_ssid) - 1);
            }
            if (config["drone"]["password"]) {
                std::string pwd = config["drone"]["password"].as<std::string>();
                std::strncpy(m_password, pwd.c_str(), sizeof(m_password) - 1);
            }
        }
        if (config["features"]) {
            if (config["features"]["slam_enabled"]) {
                m_slamEnabled = config["features"]["slam_enabled"].as<bool>();
                m_slamEngine.setEnabled(m_slamEnabled);
            }
            if (config["features"]["sam_enabled"]) {
                m_aiSegmentation = config["features"]["sam_enabled"].as<bool>();
                if (m_segmentation) m_segmentation->setEnabled(m_aiSegmentation);
            }
        }
    } catch (const YAML::Exception& e) {
        spdlog::warn("SettingsPanel: Could not load config/settings.yaml ({})", e.what());
    }
}

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
        
        if (ImGui::CollapsingHeader("AI / Processing", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Checkbox("Instance Segmentation (YOLOv11)", &m_aiSegmentation)) {
                if (m_segmentation) m_segmentation->setEnabled(m_aiSegmentation);
            }
            ImGui::Separator();
            
            if (ImGui::Checkbox("Monocular SLAM (Map Viewer)", &m_slamEnabled)) {
                m_slamEngine.setEnabled(m_slamEnabled);
            }
            
            if (m_aiSegmentation || m_slamEnabled) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Warning: AI processing consumes CPU");
            }
        }
    }
    ImGui::End();
}
