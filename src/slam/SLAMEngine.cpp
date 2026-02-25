#include "SLAMEngine.h"
#include <iostream>

SLAMEngine::SLAMEngine() : m_enabled(false) {
    m_currentPose = Eigen::Matrix4f::Identity();
}

bool SLAMEngine::initialize(const std::string& configPath) {
    std::cout << "SLAMEngine initialized (stub)" << std::endl;
    // TODO: Create ORB_SLAM3::System instance here
    return true;
}

void SLAMEngine::shutdown() {
    // TODO: m_slamSystem->Shutdown()
}

void SLAMEngine::processFrame(const cv::Mat& frame, const TelemetryData& imu) {
    if (!m_enabled || frame.empty()) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    // --- PLACEHOLDER ---
    // In a real monocular-inertial SLAM system:
    // 1. Pass IMU measurements since last frame: system->TrackMonocular(frame, timestamp, imuMeasurements)
    // 2. The system handles feature extraction, tracking, local mapping, and loop closing
    // 3. Extract pose: cv::Mat Tcw = system->TrackMonocular(...)

    // For the stub, just drift the pose slightly based on the IMU velocity to prove it works
    float dt = 1.0f / 30.0f; // Approx 30fps
    m_currentPose(0, 3) += (imu.vgx / 100.0f) * dt; // cm/s to m/s
    m_currentPose(1, 3) += (imu.vgy / 100.0f) * dt;
    m_currentPose(2, 3) += (imu.vgz / 100.0f) * dt;

    // Optional: add some random points to the map so the viewer has something to draw
    if (m_mapPoints.size() < 100) {
        float rx = ((rand() % 100) / 50.0f - 1.0f) * 5.0f;
        float ry = ((rand() % 100) / 50.0f - 1.0f) * 5.0f;
        float rz = ((rand() % 100) / 50.0f) * 5.0f; // positive Z (forward)
        m_mapPoints.push_back(Eigen::Vector3f(rx, ry, rz));
    }
}

Eigen::Matrix4f SLAMEngine::getPose() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentPose; // Thread-safe copy
}

std::vector<Eigen::Vector3f> SLAMEngine::getMapPoints() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_mapPoints; // Thread-safe copy
}
