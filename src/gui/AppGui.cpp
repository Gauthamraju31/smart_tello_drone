#include "AppGui.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <GLFW/glfw3.h>
#include <iostream>

AppGui::AppGui(GLFWwindow* window, TelloSDK& sdk, VideoDecoder& decoder, TelemetryLogger& logger, Recorder& recorder)
    : m_window(window), m_sdk(sdk), m_decoder(decoder), m_logger(logger), m_recorder(recorder) {}

void AppGui::render() {
    handleKeyboardShortcuts();
    renderDockSpace();
    renderMenuBar();
}

void AppGui::renderDockSpace() {
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);

    ImGuiID dockspace_id = ImGui::GetID("AppDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    if (m_firstTime) {
        m_firstTime = false;
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

        // Define layout
        ImGuiID dock_main_id = dockspace_id;
        ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.30f, nullptr, &dock_main_id);
        ImGuiID dock_right_bottom_id = ImGui::DockBuilderSplitNode(dock_right_id, ImGuiDir_Down, 0.35f, nullptr, &dock_right_id);
        ImGuiID dock_bottom_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);

        ImGui::DockBuilderDockWindow("Video Stream", dock_main_id);
        ImGui::DockBuilderDockWindow("SLAM Map Viewer", dock_main_id);
        ImGui::DockBuilderDockWindow("Telemetry", dock_right_id);
        ImGui::DockBuilderDockWindow("Flight Controls", dock_right_bottom_id);
        ImGui::DockBuilderDockWindow("Replay Controls", dock_right_bottom_id);
        ImGui::DockBuilderDockWindow("Log Terminal", dock_bottom_id);
        ImGui::DockBuilderDockWindow("Recording", dock_bottom_id);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::End();
}

void AppGui::renderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Settings", nullptr, m_showSettings)) m_showSettings = !m_showSettings;
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                if (m_window) glfwSetWindowShouldClose(m_window, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Drone")) {
            if (ImGui::MenuItem("Connect")) m_sdk.connect();
            if (ImGui::MenuItem("Disconnect")) m_sdk.disconnect();
            ImGui::Separator();
            if (ImGui::MenuItem("Takeoff", "Space")) m_sdk.takeoff();
            if (ImGui::MenuItem("Land", "L")) m_sdk.land();
            if (ImGui::MenuItem("Emergency Stop!", "Esc")) m_sdk.emergency();
            ImGui::Separator();
            if (ImGui::MenuItem("Stream On")) m_sdk.streamOn();
            if (ImGui::MenuItem("Stream Off")) m_sdk.streamOff();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Video", nullptr, &m_showVideo);
            ImGui::MenuItem("Controls", nullptr, &m_showControls);
            ImGui::MenuItem("Telemetry", nullptr, &m_showTelemetry);
            ImGui::MenuItem("Logs", nullptr, &m_showLogs);
            ImGui::MenuItem("Recording", nullptr, &m_showRecording);
            ImGui::EndMenu();
        }
        
        // Show connection status on the far right
        auto state = m_sdk.getState();
        std::string stateStr = "Disconnected";
        ImVec4 color = ImVec4(1,0,0,1);
        if (state == TelloSDK::State::Connected) { stateStr = "Connected"; color = ImVec4(1,1,0,1); }
        else if (state == TelloSDK::State::Streaming) { stateStr = "Streaming"; color = ImVec4(0,1,0,1); }
        else if (state == TelloSDK::State::Flying) { stateStr = "Flying"; color = ImVec4(0,1,1,1); }
        
        float right_offset = ImGui::CalcTextSize(stateStr.c_str()).x + 20.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - right_offset);
        ImGui::TextColored(color, "%s", stateStr.c_str());

        ImGui::EndMainMenuBar();
    }
}

void AppGui::handleKeyboardShortcuts() {
    // Only process flight hotkeys if we are not typing in a text box
    if (ImGui::GetIO().WantTextInput) return;

    if (ImGui::IsKeyPressed(ImGuiKey_Space)) m_sdk.takeoff();
    if (ImGui::IsKeyPressed(ImGuiKey_L)) m_sdk.land();
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) m_sdk.emergency();
}
