#include <iostream>
#include <chrono>
#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <opencv2/opencv.hpp>
#include <fstream>
#include <sstream>
#include "utils/Logger.h"
#include "slam/SLAMEngine.h"
#include "slam/MapViewer.h"
#include "core/TelemetryLogger.h"

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

// Parse telemetry CSV (same format as TelemetryLogger/ReplaySession)
static std::vector<TelemetryData> loadTelemetryCSV(const std::string& path) {
    std::vector<TelemetryData> data;
    std::ifstream file(path);
    if (!file.is_open()) return data;
    
    std::string line;
    std::getline(file, line); // skip header
    
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        TelemetryData t = {0};
        try {
            // Timestamp,Pitch,Roll,Yaw,VelX,VelY,VelZ,AccelX,AccelY,AccelZ,TempLow,TempHigh,ToF,Height,Battery,Barometer,FlightTime
            std::getline(ss, token, ','); t.timestamp_ms = std::stoll(token);
            std::getline(ss, token, ','); t.pitch = std::stoi(token);
            std::getline(ss, token, ','); t.roll = std::stoi(token);
            std::getline(ss, token, ','); t.yaw = std::stoi(token);
            std::getline(ss, token, ','); t.vgx = std::stoi(token);
            std::getline(ss, token, ','); t.vgy = std::stoi(token);
            std::getline(ss, token, ','); t.vgz = std::stoi(token);
            std::getline(ss, token, ','); t.agx = std::stof(token);
            std::getline(ss, token, ','); t.agy = std::stof(token);
            std::getline(ss, token, ','); t.agz = std::stof(token);
            std::getline(ss, token, ','); t.templ = std::stoi(token);
            std::getline(ss, token, ','); t.temph = std::stoi(token);
            std::getline(ss, token, ','); t.tof = std::stoi(token);
            std::getline(ss, token, ','); t.h = std::stoi(token);
            std::getline(ss, token, ','); t.bat = std::stoi(token);
            std::getline(ss, token, ','); t.baro = std::stof(token);
            std::getline(ss, token, ','); t.flightTime = std::stoi(token);
            data.push_back(t);
        } catch (...) {}
    }
    return data;
}

int main(int argc, char** argv) {
    std::string videoSource = "0";
    std::string telemetryPath = "";
    
    if (argc > 1) videoSource = argv[1];
    if (argc > 2) telemetryPath = argv[2];

    Logger::init();
    spdlog::info("Starting Standalone SLAM GUI Test...");

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "SLAM GUI Test", nullptr, nullptr);
    if (window == nullptr) return 1;
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigWindowsMoveFromTitleBarOnly = true; // Prevent window drag when interacting with 3D viewports

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    SLAMEngine slamEngine;
    MapViewer mapViewer;

    slamEngine.setEnabled(true);
    if (!slamEngine.initialize("config/tello_calib.yaml")) {
        spdlog::error("Failed to initialize SLAM Engine");
        // Could continue but SLAM won't work well without config
    }

    if (mapViewer.loadModel("model/dji_tello.glb")) {
        spdlog::info("Loaded DJI Tello 3D model successfully.");
    } else {
        spdlog::warn("Could not load model/dji_tello.glb");
    }

    // Load telemetry CSV if provided
    std::vector<TelemetryData> telemetryData;
    bool hasTelemetry = false;
    if (!telemetryPath.empty()) {
        telemetryData = loadTelemetryCSV(telemetryPath);
        hasTelemetry = !telemetryData.empty();
        if (hasTelemetry) {
            spdlog::info("Loaded {} telemetry samples from {}", telemetryData.size(), telemetryPath);
        } else {
            spdlog::warn("Failed to load telemetry from {}", telemetryPath);
        }
    }
    
    // Open video source
    bool hasVideo = true;
    cv::VideoCapture cap;
    if (videoSource.length() == 1 && std::isdigit(videoSource[0])) {
        cap.open(std::stoi(videoSource));
    } else {
        cap.open(videoSource);
    }

    if (!cap.isOpened()) {
        if (hasTelemetry) {
            spdlog::warn("No video source, using telemetry-only mode.");
            hasVideo = false;
        } else {
            spdlog::error("Could not open video source: {}", videoSource);
            return -1;
        }
    }
    
    // Auto-select mode based on available inputs
    if (hasVideo && hasTelemetry) {
        slamEngine.setPoseMode(PoseMode::FUSED);
        spdlog::info("Mode: FUSED (video + telemetry)");
    } else if (hasTelemetry) {
        slamEngine.setPoseMode(PoseMode::TELEMETRY_ONLY);
        spdlog::info("Mode: TELEMETRY_ONLY");
    } else {
        slamEngine.setPoseMode(PoseMode::VIDEO_ONLY);
        spdlog::info("Mode: VIDEO_ONLY");
    }
    
    size_t telemetryIdx = 0;
    int64_t telemetryStartTs = (hasTelemetry && !telemetryData.empty()) ? telemetryData[0].timestamp_ms : 0;
    // Wall-clock fallback for telemetry-only mode (no video)
    auto playbackStartTime = std::chrono::steady_clock::now();
    
    // Auto-compute sync offset from filename timestamps (YYYYMMDD_HHMMSS pattern)
    // e.g., vid_20260225_220421.mp4 and telemetry_20260225_220359.csv → offset = 22s
    float syncOffsetSec = 0.0f;
    {
        auto parseTimestamp = [](const std::string& path) -> int64_t {
            // Extract basename
            std::string name = path;
            auto slashPos = name.rfind('/');
            if (slashPos != std::string::npos) name = name.substr(slashPos + 1);
            
            // Find YYYYMMDD_HHMMSS pattern using regex-free search
            // Look for 8 digits, underscore, 6 digits
            for (size_t i = 0; i + 15 <= name.size(); i++) {
                bool allDigits = true;
                for (int d = 0; d < 8 && allDigits; d++) allDigits = std::isdigit(name[i+d]);
                if (!allDigits || name[i+8] != '_') continue;
                for (int d = 9; d < 15 && allDigits; d++) allDigits = std::isdigit(name[i+d]);
                if (!allDigits) continue;
                
                int hour   = std::stoi(name.substr(i+9, 2));
                int minute = std::stoi(name.substr(i+11, 2));
                int second = std::stoi(name.substr(i+13, 2));
                int day    = std::stoi(name.substr(i+6, 2));
                // Return seconds-of-day + day offset (enough for same-day comparison)
                return (int64_t)day * 86400 + hour * 3600 + minute * 60 + second;
            }
            return -1;
        };
        
        if (hasVideo && hasTelemetry) {
            int64_t videoTs = parseTimestamp(videoSource);
            int64_t telemetryTs = parseTimestamp(telemetryPath);
            if (videoTs >= 0 && telemetryTs >= 0) {
                syncOffsetSec = (float)(videoTs - telemetryTs);
                spdlog::info("Auto sync offset: {:.0f}s (video started {:.0f}s after telemetry)", 
                    syncOffsetSec, syncOffsetSec);
            }
        }
    }
    
    // Get video FPS for frame timing
    double videoFps = hasVideo ? cap.get(cv::CAP_PROP_FPS) : 30.0;
    if (videoFps <= 0) videoFps = 30.0;

    // Main loop
    cv::Mat frame;
    GLuint cameraTex = 0;
    glGenTextures(1, &cameraTex);
    glBindTexture(GL_TEXTURE_2D, cameraTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // RGB = 3 bytes per pixel, doesn't align to default 4-byte boundary
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    bool hasFrame = false;
    bool paused = false;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Advance telemetry synced to video time (or wall-clock if no video)
        TelemetryData currentTelemetry = {0};
        currentTelemetry.timestamp_ms = cv::getTickCount() * 1000.0 / cv::getTickFrequency();
        
        if (hasTelemetry && !paused && telemetryIdx < telemetryData.size()) {
            int64_t elapsedMs = 0;
            
            if (hasVideo) {
                // Video drives the clock: use video position for elapsed time
                double videoPosMsec = cap.get(cv::CAP_PROP_POS_MSEC);
                elapsedMs = (int64_t)videoPosMsec + (int64_t)(syncOffsetSec * 1000.0f);
            } else {
                // No video: wall-clock fallback
                auto now = std::chrono::steady_clock::now();
                elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - playbackStartTime).count();
            }
            
            int64_t targetTs = telemetryStartTs + elapsedMs;
            
            // Advance and process all samples up to target time
            while (telemetryIdx < telemetryData.size() && 
                   telemetryData[telemetryIdx].timestamp_ms <= targetTs) {
                currentTelemetry = telemetryData[telemetryIdx];
                if (slamEngine.isEnabled()) {
                    slamEngine.processTelemetry(currentTelemetry);
                }
                telemetryIdx++;
            }
        } else if (hasTelemetry && telemetryIdx > 0) {
            currentTelemetry = telemetryData[telemetryIdx > 0 ? telemetryIdx - 1 : 0];
        }
        

        // Capture and process video frame
        if (hasVideo && !paused && cap.read(frame)) {
            hasFrame = true;
            if (slamEngine.isEnabled() && slamEngine.getPoseMode() != PoseMode::TELEMETRY_ONLY) {
                slamEngine.processFrame(frame, currentTelemetry);
            }

            // Update camera texture — draw tracked keypoints on the frame
            cv::Mat displayFrame = frame.clone();
            auto trackedPts = slamEngine.getTrackedPoints();
            for (const auto& pt : trackedPts) {
                cv::circle(displayFrame, pt, 3, cv::Scalar(0, 255, 0), -1); // Green filled circles
            }
            // Put tracked point count on the frame
            cv::putText(displayFrame, 
                "Tracked: " + std::to_string(trackedPts.size()), 
                cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);

            cv::Mat rgbFrame;
            cv::cvtColor(displayFrame, rgbFrame, cv::COLOR_BGR2RGB);
            if (!rgbFrame.isContinuous()) rgbFrame = rgbFrame.clone();

            glBindTexture(GL_TEXTURE_2D, cameraTex);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgbFrame.cols, rgbFrame.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbFrame.data);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

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

        // Render Camera Feed
        ImGui::Begin("Camera Feed", nullptr);
        ImVec2 cam_avail_size = ImGui::GetContentRegionAvail();
        if (cam_avail_size.x > 0 && cam_avail_size.y > 0 && hasFrame) {
             // Keep aspect ratio
            float aspect = (float)frame.cols / (float)frame.rows;
            float w = cam_avail_size.x;
            float h = w / aspect;
            if (h > cam_avail_size.y) {
                h = cam_avail_size.y;
                w = h * aspect;
            }
            ImGui::Image((ImTextureID)(intptr_t)cameraTex, ImVec2(w, h));
        }
        ImGui::End();
        
        // Render Tools/Info
        ImGui::Begin("Info", nullptr);
        ImGui::Text("Video Source: %s", hasVideo ? videoSource.c_str() : "(none)");
        if (hasFrame) ImGui::Text("Frame Size: %d x %d", frame.cols, frame.rows);
        ImGui::Text("Telemetry: %s", hasTelemetry ? telemetryPath.c_str() : "(none)");
        if (hasTelemetry) {
            ImGui::Text("Telemetry: %zu / %zu", telemetryIdx, telemetryData.size());
        }
        ImGui::Text("Tracked Points: %zu", slamEngine.getTrackedPoints().size());
        ImGui::Text("Map Points: %zu", slamEngine.getMapPoints().size());
        ImGui::Text("Height: %.0f cm", slamEngine.getHeight());
        
        ImGui::Separator();
        
        // Pose Mode selector
        const char* modeNames[] = { "Video Only", "Telemetry Only", "Fused" };
        int currentMode = (int)slamEngine.getPoseMode();
        if (ImGui::Combo("Pose Mode", &currentMode, modeNames, 3)) {
            slamEngine.setPoseMode((PoseMode)currentMode);
        }
        
        // Show telemetry details if available
        if (hasTelemetry && telemetryIdx > 0 && telemetryIdx <= telemetryData.size()) {
            const auto& t = telemetryData[telemetryIdx > 0 ? telemetryIdx - 1 : 0];
            ImGui::Text("Yaw: %d  Pitch: %d  Roll: %d", t.yaw, t.pitch, t.roll);
            ImGui::Text("Vel: [%d, %d, %d] cm/s", t.vgx, t.vgy, t.vgz);
            ImGui::Text("H: %d cm  ToF: %d cm  Bat: %d%%", t.h, t.tof, t.bat);
        }
        
        // Sync offset slider (only relevant when both video and telemetry present)
        if (hasVideo && hasTelemetry) {
            ImGui::Separator();
            ImGui::SliderFloat("Sync Offset (s)", &syncOffsetSec, -30.0f, 30.0f, "%.1f s");
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset##offset")) syncOffsetSec = 0.0f;
            
            // Show sync status
            double videoTimeSec = hasVideo ? cap.get(cv::CAP_PROP_POS_MSEC) / 1000.0 : 0;
            double teleTimeSec = (telemetryIdx > 0 && telemetryIdx <= telemetryData.size())
                ? (telemetryData[telemetryIdx - 1].timestamp_ms - telemetryStartTs) / 1000.0 : 0;
            ImGui::Text("Video: %.1fs  Telem: %.1fs", videoTimeSec, teleTimeSec);
        }
        
        ImGui::Separator();
        ImGui::Text("%.1f FPS (%.3f ms)", 
            ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
        
        // Pause / Resume
        if (ImGui::Button(paused ? "Resume" : "Pause")) {
            paused = !paused;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset SLAM")) {
            slamEngine.setEnabled(false);
            slamEngine.setEnabled(true);
            slamEngine.initialize("config/tello_calib.yaml");
            cap.set(cv::CAP_PROP_POS_FRAMES, 0);
            hasFrame = false;
            paused = false;
            telemetryIdx = 0;
            playbackStartTime = std::chrono::steady_clock::now();
        }
        
        ImGui::Separator();
        
        // Export Point Cloud as PLY
        if (ImGui::Button("Export Point Cloud (.ply)")) {
            auto mapPts = slamEngine.getMapPoints();
            if (!mapPts.empty()) {
                std::string filename = "slam_pointcloud.ply";
                std::ofstream ply(filename);
                ply << "ply\n";
                ply << "format ascii 1.0\n";
                ply << "element vertex " << mapPts.size() << "\n";
                ply << "property float x\n";
                ply << "property float y\n";
                ply << "property float z\n";
                ply << "property uchar red\n";
                ply << "property uchar green\n";
                ply << "property uchar blue\n";
                ply << "end_header\n";
                for (const auto& p : mapPts) {
                    ply << p.position.x() << " " 
                        << p.position.y() << " " 
                        << p.position.z() << " "
                        << (int)(p.r * 255) << " " 
                        << (int)(p.g * 255) << " " 
                        << (int)(p.b * 255) << "\n";
                }
                ply.close();
                spdlog::info("Exported {} points to {}", mapPts.size(), filename);
            } else {
                spdlog::warn("No map points to export.");
            }
        }
        if (slamEngine.getMapPoints().size() > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%zu pts)", slamEngine.getMapPoints().size());
        }
        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
