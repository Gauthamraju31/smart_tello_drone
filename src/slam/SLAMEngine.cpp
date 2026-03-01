#include "SLAMEngine.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <iostream>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

SLAMEngine::SLAMEngine() : m_enabled(false), m_isFirstFrame(true), m_lastTimestamp(0) {
    reset();
}

void SLAMEngine::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentPose = Eigen::Matrix4f::Identity();
    m_mapPoints.clear();
    m_isFirstFrame = true;
    m_R_f = cv::Mat::eye(3, 3, CV_64F);
    m_t_f = cv::Mat::zeros(3, 1, CV_64F);
    m_prevPoints.clear();
    m_lastTimestamp = 0;
    
    // Dead reckoning reset
    m_drPose = Eigen::Matrix4f::Identity();
    m_drPosition = Eigen::Vector3f::Zero();
    m_drInitialized = false;
    m_initialYaw = 0.0f;
    m_currentHeight = 0.0f;
}

#include <yaml-cpp/yaml.h>

bool SLAMEngine::initialize(const std::string& configPath) {
    try {
        YAML::Node config = YAML::LoadFile(configPath);
        if (config["camera_matrix"] && config["camera_matrix"]["data"]) {
            std::vector<double> cam_data = config["camera_matrix"]["data"].as<std::vector<double>>();
            if (cam_data.size() == 9) {
                m_K = (cv::Mat_<double>(3, 3) << 
                       cam_data[0], cam_data[1], cam_data[2],
                       cam_data[3], cam_data[4], cam_data[5],
                       cam_data[6], cam_data[7], cam_data[8]);
                std::cout << "[info] SLAMEngine (OpenCV VO): Loaded calibration from " << configPath << std::endl;
                return true;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[warn] SLAMEngine failed to load calibration from " << configPath << ": " << e.what() << std::endl;
    }
    
    // Tello 720p approximate intrinsic matrix fallback
    std::cout << "[info] SLAMEngine (OpenCV VO): Using fallback intrinsic parameters." << std::endl;
    double focal_length = 920.0;
    m_K = (cv::Mat_<double>(3, 3) << focal_length, 0, 480.0, 
                                     0, focal_length, 360.0, 
                                     0, 0, 1);
    
    return true;
}

void SLAMEngine::shutdown() {
    reset();
}

// ============================================================
// Dead Reckoning from Telemetry
// ============================================================
void SLAMEngine::updateDeadReckoningPose(const TelemetryData& imu) {
    // Tello telemetry:
    //   yaw, pitch, roll: degrees (attitude)
    //   vgx, vgy, vgz: cm/s (velocity in body frame)
    //   h: height in cm (from downward IR/ToF sensor)
    //   tof: time-of-flight distance cm
    
    m_currentHeight = (imu.h > 0) ? imu.h : (imu.tof > 0 ? imu.tof : m_currentHeight);
    
    if (!m_drInitialized) {
        m_initialYaw = imu.yaw;
        m_drInitialized = true;
        m_lastTimestamp = imu.timestamp_ms;
        return;
    }
    
    float dt = (imu.timestamp_ms - m_lastTimestamp) / 1000.0f;
    if (dt <= 0) dt = 1.0f / 30.0f;
    if (dt > 1.0f) dt = 1.0f / 30.0f; // Clamp for large gaps
    m_lastTimestamp = imu.timestamp_ms;
    
    // Attitude angles (degrees -> radians)
    // Tello yaw: clockwise positive when viewed from above
    float yawRad   = (imu.yaw - m_initialYaw) * M_PI / 180.0f;
    float pitchRad = imu.pitch * M_PI / 180.0f;
    float rollRad  = imu.roll * M_PI / 180.0f;
    
    // Build rotation matrix from Euler angles
    // In CV convention: Y-down, Z-forward
    // Yaw rotates around Y (vertical axis)
    Eigen::AngleAxisf yawRot(-yawRad, Eigen::Vector3f::UnitY()); // Negate for CV Y-down convention
    Eigen::AngleAxisf pitchRot(pitchRad, Eigen::Vector3f::UnitX());
    Eigen::AngleAxisf rollRot(rollRad, Eigen::Vector3f::UnitZ());
    Eigen::Matrix3f R_body = (yawRot * pitchRot * rollRot).toRotationMatrix();
    
    // Tello body-frame velocities (cm/s -> m/s):
    //   vgx = forward, vgy = left, vgz = up
    // Map to CV world frame (X=right, Y=down, Z=forward):
    //   body forward (vgx) -> world Z
    //   body left (vgy) -> world -X  
    //   body up (vgz) -> world -Y (handled by height sensor instead)
    Eigen::Vector3f vel_body_cv(
        -imu.vgy / 100.0f,  // body left -> world -X (right is positive)
        0.0f,                // Y handled by height sensor
        imu.vgx / 100.0f    // body forward -> world Z
    );
    
    // Rotate horizontal velocity by yaw to get world-frame movement
    Eigen::Vector3f vel_world = R_body * vel_body_cv;
    m_drPosition += vel_world * dt;
    
    // Use actual height from sensor for Y position
    float heightM = m_currentHeight / 100.0f;
    
    // Build the dead-reckoning pose matrix
    m_drPose = Eigen::Matrix4f::Identity();
    m_drPose.block<3,3>(0,0) = R_body;
    m_drPose(0, 3) = m_drPosition.x();   // X: lateral movement
    m_drPose(1, 3) = -heightM;            // Y: height (negated for CV Y-down)
    m_drPose(2, 3) = m_drPosition.z();   // Z: forward movement
}

void SLAMEngine::processTelemetry(const TelemetryData& imu) {
    if (!m_enabled) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    
    updateDeadReckoningPose(imu);
    
    // In telemetry-only mode, the DR pose IS the current pose
    if (m_poseMode == PoseMode::TELEMETRY_ONLY) {
        m_currentPose = m_drPose;
    }
}

// ============================================================
// Visual Odometry + Fusion
// ============================================================
void SLAMEngine::processFrame(const cv::Mat& frame, const TelemetryData& imu) {
    if (!m_enabled || frame.empty()) return;

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Always update dead reckoning and height from telemetry
    updateDeadReckoningPose(imu);

    if (m_isFirstFrame || m_prevPoints.size() < 100) {
        cv::goodFeaturesToTrack(gray, m_prevPoints, 2000, 0.01, 10);
        if (m_prevPoints.empty()) return;
        m_prevGray = gray.clone();
        m_isFirstFrame = false;
        m_lastTimestamp = imu.timestamp_ms;
        return;
    }

    // Measure dt
    float dt = (imu.timestamp_ms - m_lastTimestamp) / 1000.0f;
    if (dt <= 0) dt = 1.0f / 30.0f; 
    m_lastTimestamp = imu.timestamp_ms;

    // Track features via Lucas-Kanade optical flow
    std::vector<cv::Point2f> currPoints;
    std::vector<uchar> status;
    std::vector<float> err;
    cv::calcOpticalFlowPyrLK(m_prevGray, gray, m_prevPoints, currPoints, status, err);

    std::vector<cv::Point2f> goodPrev, goodCurr;
    for (size_t i = 0; i < status.size(); i++) {
        if (status[i]) {
            goodPrev.push_back(m_prevPoints[i]);
            goodCurr.push_back(currPoints[i]);
        }
    }

    if (goodPrev.size() < 15) {
        m_isFirstFrame = true;
        return;
    }

    // Motion Estimation (Essential Matrix)
    cv::Mat mask;
    cv::Mat E = cv::findEssentialMat(goodCurr, goodPrev, m_K, cv::RANSAC, 0.999, 1.0, mask);
    
    if (E.rows != 3 || E.cols != 3) {
        m_isFirstFrame = true;
        return;
    }

    cv::Mat R, t;
    int inliers = cv::recoverPose(E, goodCurr, goodPrev, m_K, R, t, mask);

    if (inliers < 10) {
        m_isFirstFrame = true;
        return;
    }

    // Scale resolution depends on mode
    float absolute_scale = 0.05f; // fallback
    
    float vx = imu.vgx / 100.0f;
    float vy = imu.vgy / 100.0f;
    float vz = imu.vgz / 100.0f;
    float imu_scale = std::sqrt(vx*vx + vy*vy + vz*vz) * dt;
    
    if (m_poseMode == PoseMode::FUSED && imu_scale > 0.005f) {
        // Use telemetry velocity for accurate scale
        absolute_scale = imu_scale;
    } else if (m_poseMode == PoseMode::VIDEO_ONLY) {
        if (imu_scale > 0.005f) {
            absolute_scale = imu_scale;
        }
        // else use fallback
    }

    // Always update VO rotation
    m_R_f = R * m_R_f;
    m_t_f = m_t_f + absolute_scale * (m_R_f * t);
    
    // Build VO pose
    Eigen::Matrix4f voPose = Eigen::Matrix4f::Identity();
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            voPose(r, c) = m_R_f.at<double>(r, c);
        }
        voPose(r, 3) = m_t_f.at<double>(r);
    }
    voPose(3, 3) = 1.0f;

    // Select output pose based on mode
    switch (m_poseMode) {
        case PoseMode::VIDEO_ONLY:
            m_currentPose = voPose;
            break;
        case PoseMode::TELEMETRY_ONLY:
            m_currentPose = m_drPose;
            break;
        case PoseMode::FUSED: {
            // Fused: use VO rotation (more precise) with telemetry scale
            // Translation: use telemetry-scaled VO translation for XZ,
            //              use actual height sensor for Y
            m_currentPose = voPose;
            float heightM = m_currentHeight / 100.0f;
            if (heightM > 0.01f) {
                m_currentPose(1, 3) = -heightM; // Y = -height in CV convention
            }
            break;
        }
    }

    // Depth estimation for map points
    // Use actual height from sensor if available, otherwise fallback
    double depth = 2.0;
    if (m_currentHeight > 5.0f) {
        depth = m_currentHeight / 100.0; // Convert cm to meters
    }

    // Generate Map Points
    if (m_mapPoints.size() < 4000) {
        for (size_t i = 0; i < goodCurr.size(); i++) {
            if (mask.at<uchar>(i) && rand() % 10 == 0) {
                double x = (goodCurr[i].x - m_K.at<double>(0, 2)) / m_K.at<double>(0, 0);
                double y = (goodCurr[i].y - m_K.at<double>(1, 2)) / m_K.at<double>(1, 1);
                
                cv::Mat pt3d_cam = (cv::Mat_<double>(3, 1) << x * depth, y * depth, depth);
                cv::Mat pt3d_world = m_R_f.t() * (pt3d_cam - m_t_f);
                
                ColoredMapPoint cpt;
                cpt.position = Eigen::Vector3f(
                    pt3d_world.at<double>(0),
                    pt3d_world.at<double>(1),
                    pt3d_world.at<double>(2)
                );
                int px = std::clamp((int)goodCurr[i].x, 0, frame.cols - 1);
                int py = std::clamp((int)goodCurr[i].y, 0, frame.rows - 1);
                cv::Vec3b bgr = frame.at<cv::Vec3b>(py, px);
                cpt.r = bgr[2] / 255.0f;
                cpt.g = bgr[1] / 255.0f;
                cpt.b = bgr[0] / 255.0f;
                
                m_mapPoints.push_back(cpt);
            }
        }
    }

    m_prevPoints = goodCurr;
    m_prevGray = gray.clone();
}

Eigen::Matrix4f SLAMEngine::getPose() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentPose;
}

std::vector<ColoredMapPoint> SLAMEngine::getMapPoints() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_mapPoints;
}

std::vector<cv::Point2f> SLAMEngine::getTrackedPoints() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_prevPoints;
}
