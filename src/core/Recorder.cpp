#include "Recorder.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <filesystem>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

namespace {
    int64_t now_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
}

Recorder::Recorder() : m_isRecording(false), m_stopRequested(false), m_startTime_ms(0) {
    if (!std::filesystem::exists("recordings")) {
        std::filesystem::create_directory("recordings");
    }
}

Recorder::~Recorder() {
    stopRecording();
}

std::string Recorder::generateFilename(const std::string& prefix, const std::string& ext) const {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << "recordings/" << prefix << "_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ext;
    return oss.str();
}

bool Recorder::startRecording(int width, int height, double fps) {
    if (m_isRecording) return true;

    std::string filename = generateFilename("vid", ".mp4");
    
    // Use H.264 encoding via OpenCV
    int fourcc = cv::VideoWriter::fourcc('a', 'v', 'c', '1');
    
    if (!m_writer.open(filename, fourcc, fps, cv::Size(width, height), true)) {
        // Fallback to MP4V if AVC1 fails
        fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
        if (!m_writer.open(filename, fourcc, fps, cv::Size(width, height), true)) {
            std::cerr << "Failed to open VideoWriter for " << filename << std::endl;
            return false;
        }
    }

    m_isRecording = true;
    m_stopRequested = false;
    m_startTime_ms = now_ms();

    // Clear queue
    std::queue<VideoFrame> empty;
    std::swap(m_frameQueue, empty);

    m_recordingThread = std::thread(&Recorder::recordingThreadFunc, this);
    
    std::cout << "Started recording to " << filename << std::endl;
    return true;
}

void Recorder::stopRecording() {
    if (!m_isRecording) return;
    
    m_stopRequested = true;
    m_cv.notify_one();
    
    if (m_recordingThread.joinable()) {
        m_recordingThread.join();
    }
    
    m_isRecording = false;
    std::cout << "Stopped recording." << std::endl;
}

bool Recorder::isRecording() const {
    return m_isRecording.load();
}

double Recorder::getRecordingDuration() const {
    if (!m_isRecording) return 0.0;
    return (now_ms() - m_startTime_ms) / 1000.0;
}

bool Recorder::takeSnapshot(const VideoFrame& frame) {
    if (frame.data.empty() || frame.width <= 0 || frame.height <= 0) return false;

    cv::Mat rgbMat(frame.height, frame.width, CV_8UC3, (void*)frame.data.data());
    cv::Mat bgrMat;
    cv::cvtColor(rgbMat, bgrMat, cv::COLOR_RGB2BGR); // OpenCV expects BGR for saving

    std::string filename = generateFilename("snap", ".png");
    bool success = cv::imwrite(filename, bgrMat);
    
    if (success) {
        std::cout << "Saved snapshot to " << filename << std::endl;
    } else {
        std::cerr << "Failed to save snapshot" << std::endl;
    }
    return success;
}

void Recorder::addFrame(const VideoFrame& frame) {
    if (!m_isRecording || m_stopRequested) return;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_frameQueue.push(frame); // copy
        
        // Prevent unbounded growth if disk gets slow
        if (m_frameQueue.size() > 100) {
            m_frameQueue.pop();
        }
    }
    m_cv.notify_one();
}

void Recorder::recordingThreadFunc() {
    while (m_isRecording) {
        VideoFrame frame;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_cv.wait(lock, [this]{ return !m_frameQueue.empty() || m_stopRequested; });
            
            if (m_frameQueue.empty() && m_stopRequested) break;
            if (m_frameQueue.empty()) continue;
            
            frame = m_frameQueue.front();
            m_frameQueue.pop();
        }

        if (!frame.data.empty() && m_writer.isOpened()) {
            cv::Mat rgbMat(frame.height, frame.width, CV_8UC3, (void*)frame.data.data());
            cv::Mat bgrMat;
            cv::cvtColor(rgbMat, bgrMat, cv::COLOR_RGB2BGR);
            m_writer.write(bgrMat);
        }
    }

    // Flush remaining frames
    while (true) {
        VideoFrame frame;
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_frameQueue.empty()) break;
            frame = m_frameQueue.front();
            m_frameQueue.pop();
        }
        if (!frame.data.empty() && m_writer.isOpened()) {
            cv::Mat rgbMat(frame.height, frame.width, CV_8UC3, (void*)frame.data.data());
            cv::Mat bgrMat;
            cv::cvtColor(rgbMat, bgrMat, cv::COLOR_RGB2BGR);
            m_writer.write(bgrMat);
        }
    }

    if (m_writer.isOpened()) {
        m_writer.release();
    }
}
