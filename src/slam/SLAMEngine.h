#pragma once

#include "../core/TelemetryLogger.h" // For IMU data
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/video/tracking.hpp>
#include <opencv2/calib3d.hpp>
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <vector>
#include <mutex>
#include <atomic>

class SLAMEngine {
public:
    SLAMEngine();
    ~SLAMEngine() = default;

    bool initialize(const std::string& configPath);
    void shutdown();

    // Process a frame + IMU data to estimate motion
    void processFrame(const cv::Mat& frame, const TelemetryData& imu);

    // Get the current camera pose relative to the start position (world frame)
    Eigen::Matrix4f getPose() const;

    // Get all map points (point cloud) for 3D visualization
    std::vector<Eigen::Vector3f> getMapPoints() const;

    bool isEnabled() const { return m_enabled.load(); }
    void setEnabled(bool enabled) { 
        m_enabled = enabled; 
        if (!enabled) reset();
    }

private:
    void reset();

    std::atomic<bool> m_enabled;
    mutable std::mutex m_mutex;

    // Camera Intrinsic Matrix (approximated for Tello 720p)
    cv::Mat m_K;
    
    // Vo State
    bool m_isFirstFrame;
    cv::Mat m_prevGray;
    std::vector<cv::Point2f> m_prevPoints;
    
    // 3D State
    Eigen::Matrix4f m_currentPose;
    cv::Mat m_R_f, m_t_f; // OpenCV format for accumulation
    std::vector<Eigen::Vector3f> m_mapPoints;
    
    // Timestamp for dt calculation
    int64_t m_lastTimestamp;
};
