#pragma once

#include "VideoDecoder.h"
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <opencv2/videoio.hpp>

class Recorder {
public:
    Recorder();
    ~Recorder();

    // Start recording video
    bool startRecording(int width, int height, double fps = 30.0);
    void stopRecording();
    bool isRecording() const;

    // Take a single snapshot
    bool takeSnapshot(const VideoFrame& frame);

    // Call this from Decoder's onFrameDecoded callback
    void addFrame(const VideoFrame& frame);

    // Get current recording duration in seconds
    double getRecordingDuration() const;

private:
    std::string generateFilename(const std::string& prefix, const std::string& ext) const;
    void recordingThreadFunc();

    std::atomic<bool> m_isRecording;
    std::atomic<bool> m_stopRequested;
    
    cv::VideoWriter m_writer;
    
    std::thread m_recordingThread;
    std::mutex m_queueMutex;
    std::condition_variable m_cv;
    std::queue<VideoFrame> m_frameQueue;

    int64_t m_startTime_ms;
};
