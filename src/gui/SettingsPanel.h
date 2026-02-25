#pragma once

#include "../core/TelloSDK.h"
#include <imgui.h>

class SettingsPanel {
public:
    SettingsPanel();
    
    void render(bool* p_open = nullptr);

private:
    char m_ssid[128] = "TELLO-XXXXXX";
    char m_password[128] = "";
    bool m_missionPadEnabled = false;
    int m_videoBitrate = 2; // Default auto
    
    bool m_aiTracking = false;
    bool m_aiSegmentation = false;
    bool m_slamEnabled = false;
};
