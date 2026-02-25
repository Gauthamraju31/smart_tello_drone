#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include "TelemetryLogger.h" // For TelemetryData struct
#include "VideoDecoder.h"    // For VideoFrame struct

class ReplaySession {
public:
    ReplaySession();
    ~ReplaySession();

    bool start(const std::string& videoPath, const std::string& telemetryPath);
    void stop();

    bool isRunning() const { return m_running.load(); }
    
    // Playback Controls
    void setPaused(bool paused) { m_paused = paused; }
    bool isPaused() const { return m_paused.load(); }
    
    float getProgress() const; // 0.0 to 1.0
    int64_t getCurrentTimeMs() const;
    int64_t getTotalDurationMs() const;
    
    void seek(float progress); // 0.0 to 1.0

    // Callbacks that mimic the live drone API
    // GUI and SLAM systems bind to these just like the TelloSDK/VideoDecoder ones
    std::function<void(const VideoFrame&)> onVideoFrame;
    std::function<void(const TelemetryData&)> onTelemetry;

private:
    void playbackThread();
    bool parseTelemetryCSV(const std::string& path);

    std::string m_videoPath;
    
    std::atomic<bool> m_running;
    std::atomic<bool> m_paused;
    std::thread m_thread;

    // Parsed telemetry data, ordered by timestamp
    std::vector<TelemetryData> m_telemetryData;
    
    // Playback state
    std::atomic<int64_t> m_currentTimeMs;
    std::atomic<int64_t> m_totalDurationMs;
    
    // Seek state
    std::atomic<bool> m_hasSeekRequest;
    std::atomic<int64_t> m_seekRequestMs;
};
