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

struct ColoredMapPoint {
    Eigen::Vector3f position;
    float r, g, b; // 0-1 range
};

enum class PoseMode {
    VIDEO_ONLY,      // Pure visual odometry
    TELEMETRY_ONLY,  // Dead reckoning from IMU velocities + attitude
    FUSED            // Telemetry for scale/height, VO for fine rotation
};

class SLAMEngine {
public:
    SLAMEngine();
    ~SLAMEngine() = default;

    bool initialize(const std::string& configPath);
    void shutdown();

    // Process a frame + IMU data to estimate motion
    void processFrame(const cv::Mat& frame, const TelemetryData& imu);

    // Process telemetry only (for dead-reckoning without video)
    void processTelemetry(const TelemetryData& imu);

    // Get the current camera pose relative to the start position (world frame)
    Eigen::Matrix4f getPose() const;

    // Get all map points (point cloud) for 3D visualization
    std::vector<ColoredMapPoint> getMapPoints() const;

    // Get current 2D tracked feature points (for overlay visualization)
    std::vector<cv::Point2f> getTrackedPoints() const;

    bool isEnabled() const { return m_enabled.load(); }
    void setEnabled(bool enabled) { 
        m_enabled = enabled; 
        if (!enabled) reset();
    }

    // Pose mode
    void setPoseMode(PoseMode mode) { m_poseMode = mode; }
    PoseMode getPoseMode() const { return m_poseMode; }

    // Get the current height estimate (cm)
    float getHeight() const { return m_currentHeight; }

private:
    void reset();
    void updateDeadReckoningPose(const TelemetryData& imu);

    std::atomic<bool> m_enabled;
    mutable std::mutex m_mutex;
    PoseMode m_poseMode = PoseMode::VIDEO_ONLY;

    // Camera Intrinsic Matrix (approximated for Tello 720p)
    cv::Mat m_K;
    
    // VO State
    bool m_isFirstFrame;
    cv::Mat m_prevGray;
    std::vector<cv::Point2f> m_prevPoints;
    
    // 3D State (Visual Odometry)
    Eigen::Matrix4f m_currentPose;  // Output pose (selected by mode)
    cv::Mat m_R_f, m_t_f;           // OpenCV format for VO accumulation
    std::vector<ColoredMapPoint> m_mapPoints;
    
    // Dead Reckoning State (Telemetry)
    Eigen::Matrix4f m_drPose;       // Dead-reckoning pose from telemetry
    Eigen::Vector3f m_drPosition;   // Accumulated position (meters)
    float m_initialYaw = 0.0f;
    bool m_drInitialized = false;
    
    // Height tracking
    float m_currentHeight = 0.0f;   // cm, from ToF/height sensor
    
    // Timestamp for dt calculation
    int64_t m_lastTimestamp;
};
