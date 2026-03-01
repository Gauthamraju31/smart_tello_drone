#pragma once

#include "../core/TelloSDK.h"
#include "../slam/SLAMEngine.h"
#include "../ai/Segmentation.h"
#include <imgui.h>
#include <memory>

class SettingsPanel {
public:
    SettingsPanel(SLAMEngine& slamEngine, std::shared_ptr<Segmentation> segmentation);
    
    void render(bool* p_open = nullptr);

private:
    char m_ssid[128] = "";
    char m_password[128] = "";
    bool m_missionPadEnabled = false;
    int m_videoBitrate = 2; // Default auto
    
    bool m_aiSegmentation = false;
    bool m_slamEnabled = false;

    SLAMEngine& m_slamEngine;
    std::shared_ptr<Segmentation> m_segmentation;
};
