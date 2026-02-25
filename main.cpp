#include <iostream>
#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "utils/Logger.h"
#include "core/TelloSDK.h"
#include "core/VideoDecoder.h"
#include "core/TelemetryLogger.h"
#include "core/Recorder.h"

#include "gui/Theme.h"
#include "gui/AppGui.h"
#include "gui/VideoWindow.h"
#include "gui/ControlPanel.h"
#include "gui/TelemetryPanel.h"
#include "gui/LogTerminal.h"
#include "gui/RecordingPanel.h"
#include "gui/SettingsPanel.h"

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main(int argc, char** argv) {
    Logger::init();
    spdlog::info("Starting Smart Tello Drone Simulator UI...");

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Smart Tello Drone", nullptr, nullptr);
    if (window == nullptr) return 1;
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    Theme::applyCatppuccinDark(); // Apply custom theme

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Core Components
    TelloSDK sdk;
    VideoDecoder decoder;
    TelemetryLogger telemetryLogger;
    Recorder recorder;

    if (!decoder.initialize()) {
        spdlog::error("Failed to initialize VideoDecoder");
        return -1;
    }

    // Wiring callbacks
    sdk.onTelemetry = [&telemetryLogger](const TelemetryData& t) {
        telemetryLogger.log(t);
    };

    sdk.onVideoData = [&decoder](const uint8_t* data, size_t size) {
        decoder.decodeNetworkPacket(data, size);
    };

    sdk.onResponse = [](const std::string& resp) {
        spdlog::debug("Tello: {}", resp);
    };

    decoder.onFrameDecoded = [&recorder](const VideoFrame& frame) {
        recorder.addFrame(frame);
    };

    // Begin logging immediately if configured, or wait for user to start it manually
    telemetryLogger.start();

    // GUI Manager
    AppGui appGui(window, sdk, decoder, telemetryLogger, recorder);
    VideoWindow videoWindow(decoder);
    ControlPanel controlPanel(sdk);
    TelemetryPanel telemetryPanel(sdk);
    LogTerminal logTerminal(Logger::getGuiSink());
    RecordingPanel recordingPanel(recorder, decoder);
    SettingsPanel settingsPanel;

    // We modify AppGui::render to take the panels as dependencies 
    // to avoid duplicating state. (A quick hack for this structure)
    // For cleaner architecture, AppGui would own or inject them cleanly.

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 1. AppGui creates Dockspace and Menu
        appGui.render(); 

        // 2. We render the individual panels if their flags in AppGui are true
        // Since AppGui owns the bools, we'll patch AppGui to call these, 
        // OR render them here. For simplicity, we just render them:
        // (Note: To keep it clean without massive refactoring, we render them all
        //  and let ImGui handle the docking visibility, but normally we'd pass bool ptrs)
        
        bool dummy = true;
        videoWindow.render(&dummy);
        controlPanel.render(&dummy);
        telemetryPanel.render(&dummy);
        logTerminal.render(&dummy);
        recordingPanel.render(&dummy);
        settingsPanel.render(&dummy);

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        //     GLFWwindow* backup_current_context = glfwGetCurrentContext();
        //     ImGui::UpdatePlatformWindows();
        //     ImGui::RenderPlatformWindowsDefault();
        //     glfwMakeContextCurrent(backup_current_context);
        // }

        glfwSwapBuffers(window);
    }

    // Cleanup
    sdk.disconnect();
    telemetryLogger.stop();
    recorder.stopRecording();
    decoder.shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
