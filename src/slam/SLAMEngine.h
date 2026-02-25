#pragma once

#include "../core/TelemetryLogger.h" // For IMU data
#include <opencv2/core.hpp>
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

    // Process a frame + IMU data synchronously
    // In a real system (like ORB-SLAM3), this tracks the monocular camera
    void processFrame(const cv::Mat& frame, const TelemetryData& imu);

    // Get the current camera pose relative to the start position (world frame)
    Eigen::Matrix4f getPose() const;

    // Get all map points (point cloud) for 3D visualization
    std::vector<Eigen::Vector3f> getMapPoints() const;

    bool isEnabled() const { return m_enabled.load(); }
    void setEnabled(bool enabled) { m_enabled = enabled; }

private:
    std::atomic<bool> m_enabled;
    mutable std::mutex m_mutex;

    // Mock state
    Eigen::Matrix4f m_currentPose;
    std::vector<Eigen::Vector3f> m_mapPoints;
    
    // Future: ORB_SLAM3::System* m_slamSystem;
};
