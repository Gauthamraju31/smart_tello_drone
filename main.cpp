#include <iostream>
#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "utils/Logger.h"
#include "utils/AudioEngine.h"
#include "core/TelloSDK.h"
#include "core/VideoDecoder.h"
#include "core/TelemetryLogger.h"
#include "core/Recorder.h"
#include "core/ReplaySession.h"
#include "slam/SLAMEngine.h"
#include "slam/MapViewer.h"
#include "ai/Segmentation.h"

#include "gui/Theme.h"
#include "gui/AppGui.h"
#include "gui/VideoWindow.h"
#include "gui/ControlPanel.h"
#include "gui/TelemetryPanel.h"
#include "gui/LogTerminal.h"
#include "gui/RecordingPanel.h"
#include "gui/SettingsPanel.h"
#include "gui/ReplayPanel.h"

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main(int argc, char** argv) {
    bool replayMode = false;
    std::string replayVideo, replayCSV;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Smart Tello Drone Simulator UI\n"
                      << "Usage: SmartTelloDrone [OPTIONS]\n\n"
                      << "Options:\n"
                      << "  --help, -h                  Show this help message and exit\n"
                      << "  --simulate                  Run with dummy data (No drone required)\n"
                      << "  --replay <video> <csv>      Replay a recorded mission using the specified\n"
                      << "                              MP4 video and CSV telemetry files.\n\n"
                      << "Examples:\n"
                      << "  ./SmartTelloDrone\n"
                      << "  ./SmartTelloDrone --simulate\n"
                      << "  ./SmartTelloDrone --replay recordings/vid_test.mp4 logs/telemetry_test.csv\n";
            return 0;
        } else if (arg == "--replay" && i + 2 < argc) {
            replayMode = true;
            replayVideo = argv[++i];
            replayCSV = argv[++i];
        }
    }
    Logger::init();
    AudioEngine::init();
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

    TelloSDK sdk;
    VideoDecoder decoder;
    TelemetryLogger telemetryLogger;
    Recorder recorder;
    SLAMEngine slamEngine;
    MapViewer mapViewer;

    slamEngine.setEnabled(true);
    slamEngine.initialize("config/tello_calib.yaml");

    if (mapViewer.loadModel("model/dji_tello.glb")) {
        spdlog::info("Loaded DJI Tello 3D model successfully.");
    } else {
        spdlog::warn("Could not load model/dji_tello.glb");
    }

    if (!decoder.initialize()) {
        spdlog::error("Failed to initialize VideoDecoder");
        return -1;
    }

    auto segmentation = std::make_shared<Segmentation>();
    segmentation->initialize();
    decoder.addProcessor(segmentation);

    ReplaySession replaySession;

    if (replayMode) {
        spdlog::info("Running in REPLAY mode.");
        replaySession.onVideoFrame = [&decoder, &recorder, &slamEngine](const VideoFrame& frame) {
            decoder.setLatestFrame(frame); // Safely passes to main thread
            recorder.addFrame(frame);
            if (slamEngine.isEnabled()) {
                cv::Mat matFrame(frame.height, frame.width, CV_8UC3, (void*)frame.data.data());
                // We don't have perfect sync in this lambda for IMU, but SLAMEngine stores the last IMU anyway
                // So we just pass a zeroed one or let SLAMEngine use the last received IMU time.
                // We'll pass a dummy here, and let onTelemetry drive the IMU scale
                TelemetryData dummy = {0};
                dummy.timestamp_ms = frame.timestamp_ms;
                slamEngine.processFrame(matFrame, dummy);
            }
        };

        // Inject telemetry
        replaySession.onTelemetry = [&telemetryLogger](const TelemetryData& t) {
            telemetryLogger.log(t);
        };

        replaySession.start(replayVideo, replayCSV);
    } else {
        // Live drone mode Wiring callbacks
        sdk.onTelemetry = [&telemetryLogger](const TelemetryData& t) {
            telemetryLogger.log(t);
        };

        sdk.onVideoData = [&decoder](const uint8_t* data, size_t size) {
            decoder.decodeNetworkPacket(data, size);
        };

        sdk.onResponse = [](const std::string& resp) {
            spdlog::debug("Tello: {}", resp);
        };

        decoder.onFrameDecoded = [&recorder, &slamEngine, &sdk](const VideoFrame& frame) {
            recorder.addFrame(frame);
            
            if (slamEngine.isEnabled()) {
                cv::Mat matFrame(frame.height, frame.width, CV_8UC3, (void*)frame.data.data());
                slamEngine.processFrame(matFrame, sdk.getLatestTelemetry());
            }
        };
    }

    // Begin logging immediately if configured, or wait for user to start it manually
    telemetryLogger.start();

    // GUI Manager
    AppGui appGui(window, sdk, decoder, telemetryLogger, recorder);
    VideoWindow videoWindow(decoder);
    ControlPanel controlPanel(sdk);
    TelemetryPanel telemetryPanel(telemetryLogger);
    LogTerminal logTerminal(Logger::getGuiSink());
    RecordingPanel recordingPanel(recorder, decoder);
    SettingsPanel settingsPanel(slamEngine, segmentation);
    ReplayPanel replayPanel(replaySession);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        appGui.render(); 

        bool dummy = true;
        videoWindow.render(&dummy);
        telemetryPanel.render(&dummy);
        logTerminal.render(&dummy);
        settingsPanel.render(&dummy);
        
        if (replayMode) {
            replayPanel.render(&dummy);
        } else {
            controlPanel.render(&dummy);
            recordingPanel.render(&dummy);
        }

        // Render SLAM Map Viewer
        ImGui::Begin("SLAM Map Viewer", nullptr);
        
        if (ImGui::CollapsingHeader("Model Adjustments")) {
            ImGui::SliderFloat("Scale", &mapViewer.m_modelScale, 0.001f, 2.0f, "%.3f");
            ImGui::SliderFloat("Rot X (Pitch)", &mapViewer.m_modelRotX, -180.0f, 180.0f, "%.1f");
            ImGui::SliderFloat("Rot Y (Yaw)", &mapViewer.m_modelRotY, -180.0f, 180.0f, "%.1f");
            ImGui::SliderFloat("Rot Z (Roll)", &mapViewer.m_modelRotZ, -180.0f, 180.0f, "%.1f");
        }

        ImVec2 avail_size = ImGui::GetContentRegionAvail();
        if (avail_size.x > 0 && avail_size.y > 0) {
            GLuint mapTex = mapViewer.renderToTexture((int)avail_size.x, (int)avail_size.y, slamEngine);
            if (mapTex) {
                // OpenGL texture coordinates are upside down for ImGui image, pass uv0/uv1 to flip
                ImGui::Image((ImTextureID)(intptr_t)mapTex, avail_size, ImVec2(0, 1), ImVec2(1, 0));
            }
        }
        ImGui::End();

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
    if (replayMode) {
        replaySession.stop();
    } else {
        sdk.disconnect();
    }
    
    telemetryLogger.stop();
    recorder.stopRecording();
    decoder.shutdown();
    AudioEngine::shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
