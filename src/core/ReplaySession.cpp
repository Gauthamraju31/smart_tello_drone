#include "ReplaySession.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>

ReplaySession::ReplaySession() : m_running(false), m_paused(false), m_currentTimeMs(0), m_totalDurationMs(0), m_hasSeekRequest(false), m_seekRequestMs(0) {}

ReplaySession::~ReplaySession() {
    stop();
}

bool ReplaySession::start(const std::string& videoPath, const std::string& telemetryPath) {
    if (m_running) return false;

    m_videoPath = videoPath;
    m_paused = false;

    if (!parseTelemetryCSV(telemetryPath)) {
        spdlog::error("ReplaySession: Failed to load telemetry CSV: {}", telemetryPath);
        return false;
    }

    m_running = true;
    m_thread = std::thread(&ReplaySession::playbackThread, this);
    
    spdlog::info("ReplaySession: Started playback for {} and {}", videoPath, telemetryPath);
    return true;
}

void ReplaySession::stop() {
    if (m_running) {
        m_running = false;
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
}

float ReplaySession::getProgress() const {
    if (m_totalDurationMs == 0) return 0.0f;
    return static_cast<float>(m_currentTimeMs.load()) / m_totalDurationMs.load();
}

int64_t ReplaySession::getCurrentTimeMs() const {
    return m_currentTimeMs.load();
}

int64_t ReplaySession::getTotalDurationMs() const {
    return m_totalDurationMs.load();
}

void ReplaySession::seek(float progress) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress >= 1.0f) progress = 0.999f;
    
    int64_t targetTimeMs = static_cast<int64_t>(progress * m_totalDurationMs.load());
    m_seekRequestMs.store(targetTimeMs);
    m_hasSeekRequest.store(true);
}

bool ReplaySession::parseTelemetryCSV(const std::string& path) {
    m_telemetryData.clear();
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    // Skip header line
    std::getline(file, line);

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        TelemetryData t = {0};

        try {
            // Timestamp,Pitch,Roll,Yaw,vGX,vGY,vGZ,aGX,aGY,aGZ,ToF,Height,Baro,Bat,Temp,Time
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
            std::getline(ss, token, ','); t.tof = std::stoi(token);
            std::getline(ss, token, ','); t.h = std::stoi(token);
            std::getline(ss, token, ','); t.baro = std::stof(token);
            std::getline(ss, token, ','); t.bat = std::stoi(token);
            std::getline(ss, token, ','); t.templ = std::stoi(token); // approximation for average temp
            t.temph = t.templ;
            std::getline(ss, token, ','); t.flightTime = std::stoi(token);

            m_telemetryData.push_back(t);
        } catch (const std::exception& e) {
            spdlog::warn("ReplaySession: Error parsing CSV line: {}", e.what());
        }
    }
    
    return true;
}

void ReplaySession::playbackThread() {
    cv::VideoCapture cap(m_videoPath);
    if (!cap.isOpened()) {
        spdlog::error("ReplaySession: Failed to open video {}", m_videoPath);
        m_running = false;
        return;
    }

    double fps = cap.get(cv::CAP_PROP_FPS);
    if (fps <= 0) fps = 30.0;
    int frameDelayMs = (int)(1000.0 / fps);
    
    // Calculate total duration
    int frameCount = cap.get(cv::CAP_PROP_FRAME_COUNT);
    if (frameCount > 0 && fps > 0) {
        m_totalDurationMs = (int64_t)((frameCount / fps) * 1000.0);
    } else {
        m_totalDurationMs = 1; // Fallback
    }

    cv::Mat frame;
    size_t teleIdx = 0;
    
    // Determine start time to synchronize playback speed
    int64_t start_time_ms = 0;
    if (!m_telemetryData.empty()) {
        start_time_ms = m_telemetryData[0].timestamp_ms;
    }

    auto real_playback_start = std::chrono::steady_clock::now();
    int64_t elapsed_ms = 0;

    while (m_running) {
        if (m_hasSeekRequest) {
            int64_t seekTimeMs = m_seekRequestMs.load();
            m_hasSeekRequest = false;

            int targetFrame = static_cast<int>((seekTimeMs / 1000.0) * fps);
            cap.set(cv::CAP_PROP_POS_FRAMES, targetFrame);
            
            teleIdx = 0;
            while (teleIdx < m_telemetryData.size() && 
                   (m_telemetryData[teleIdx].timestamp_ms - start_time_ms) < seekTimeMs) {
                teleIdx++;
            }

            real_playback_start = std::chrono::steady_clock::now() - std::chrono::milliseconds(seekTimeMs);
            elapsed_ms = seekTimeMs;
            m_currentTimeMs = elapsed_ms;
            
            // Clear any buffered frames so the next one read is definitely the desired frame
            cap.read(frame);
            continue;
        }

        if (m_paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            // Adjust the start time so that elapsed_ms doesn't jump forward when unpaused
            real_playback_start = std::chrono::steady_clock::now() - std::chrono::milliseconds(elapsed_ms);
            continue;
        }

        if (!cap.read(frame) || frame.empty()) {
            // End of stream, pause it instead of exiting the thread so user can seek back
            m_paused = true;
            m_currentTimeMs = m_totalDurationMs.load();
            continue;
        }

        // Find the telemetry matching this video frame's relative time offset
        auto now = std::chrono::steady_clock::now();
        elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - real_playback_start).count();
        m_currentTimeMs = elapsed_ms;

        // Push telemetry that occurred up to this elapsed time
        while (teleIdx < m_telemetryData.size() && 
               (m_telemetryData[teleIdx].timestamp_ms - start_time_ms) <= elapsed_ms) {
            if (onTelemetry) {
                onTelemetry(m_telemetryData[teleIdx]);
            }
            teleIdx++;
        }

        // Push Video Frame
        if (onVideoFrame) {
            VideoFrame vf;
            vf.width = frame.cols;
            vf.height = frame.rows;
            vf.timestamp_ms = start_time_ms + elapsed_ms;
            
            // OpenCV returns BGR, our system expects RGB for ImGui
            cv::Mat rgb;
            cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
            
            size_t size = rgb.total() * rgb.elemSize();
            vf.data.assign(rgb.data, rgb.data + size);
            
            onVideoFrame(vf);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(frameDelayMs));
    }

    spdlog::info("ReplaySession: Playback finished.");
    m_running = false;
    m_currentTimeMs = m_totalDurationMs.load();
}
