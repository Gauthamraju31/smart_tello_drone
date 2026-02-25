#pragma once

#include "../core/TelloSDK.h"
#include "../core/VideoDecoder.h"
#include "../core/TelemetryLogger.h"
#include "../core/Recorder.h"
#include <memory>

class AppGui {
public:
    AppGui(TelloSDK& sdk, VideoDecoder& decoder, TelemetryLogger& logger, Recorder& recorder);
    ~AppGui() = default;

    // Call this every frame inside the ImGui context
    void render();

private:
    void renderMenuBar();
    void renderDockSpace();
    
    // Panel stubs (will be moved to their own classes later)
    void renderVideoPanel();
    void renderControlPanel();
    void renderTelemetryPanel();
    void renderLogTerminal();
    void renderRecordingPanel();
    void renderSettingsPanel();
    
    void handleKeyboardShortcuts();

    TelloSDK& m_sdk;
    VideoDecoder& m_decoder;
    TelemetryLogger& m_logger;
    Recorder& m_recorder;

    // Window visibility toggles
    bool m_showVideo = true;
    bool m_showControls = true;
    bool m_showTelemetry = true;
    bool m_showLogs = true;
    bool m_showRecording = true;
    bool m_showSettings = false;
    
    // Layout flags
    bool m_firstTime = true;
};
