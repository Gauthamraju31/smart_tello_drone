#pragma once

#include "../core/TelemetryLogger.h"
#include <imgui.h>

class TelemetryPanel {
public:
    TelemetryPanel(TelemetryLogger& logger);
    
    // Renders the telemetry dashboard inside ImGui
    void render(bool* p_open = nullptr);

private:
    void renderBatteryBar(int bat);
    void renderAttitudeDial(int pitch, int roll, int yaw);
    void renderAltitudeSpeed(int h, float baro, int vgx, int vgy, int vgz);

    TelemetryLogger& m_logger;
};
