#include "SLAMEngine.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <iostream>

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

void SLAMEngine::processFrame(const cv::Mat& frame, const TelemetryData& imu) {
    if (!m_enabled || frame.empty()) return;

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_isFirstFrame || m_prevPoints.size() < 100) {
        // Find new features to track using FAST or Shi-Tomasi
        cv::goodFeaturesToTrack(gray, m_prevPoints, 2000, 0.01, 10);
        
        // If we still can't find points, wait for the next frame
        if (m_prevPoints.empty()) return;

        m_prevGray = gray.clone();
        m_isFirstFrame = false;
        m_lastTimestamp = imu.timestamp_ms;
        return;
    }

    // Measure dt for IMU scale (convert to seconds)
    float dt = (imu.timestamp_ms - m_lastTimestamp) / 1000.0f;
    if (dt <= 0) dt = 1.0f / 30.0f; 
    m_lastTimestamp = imu.timestamp_ms;

    // Track features via Lucas-Kanade optical flow
    std::vector<cv::Point2f> currPoints;
    std::vector<uchar> status;
    std::vector<float> err;
    cv::calcOpticalFlowPyrLK(m_prevGray, gray, m_prevPoints, currPoints, status, err);

    // Filter valid tracked points
    std::vector<cv::Point2f> goodPrev, goodCurr;
    for (size_t i = 0; i < status.size(); i++) {
        if (status[i]) {
            goodPrev.push_back(m_prevPoints[i]);
            goodCurr.push_back(currPoints[i]);
        }
    }

    if (goodPrev.size() < 15) {
        // Lost tracking, need to re-detect features
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

    // Monocular Scale Resolution using IMU velocity (vgx, vgy, vgz in cm/s -> m/s)
    // Absolute distance = |v| * dt
    float vx = imu.vgx / 100.0f;
    float vy = imu.vgy / 100.0f;
    float vz = imu.vgz / 100.0f;
    float absolute_scale = std::sqrt(vx*vx + vy*vy + vz*vz) * dt;

    // If drone is hovering/static, don't update translation (prevents noise drift)
    if (absolute_scale > 0.01 && t.at<double>(2) > t.at<double>(0) && t.at<double>(2) > t.at<double>(1)) {
        // Accumulate rotation and scaled translation
        m_t_f = m_t_f + absolute_scale * (m_R_f * t);
        m_R_f = R * m_R_f;
    }
    
    // Update Eigen pose matrix for MapViewer
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            m_currentPose(r, c) = m_R_f.at<double>(r, c);
        }
        m_currentPose(r, 3) = m_t_f.at<double>(r);
    }
    m_currentPose(3, 3) = 1.0f;

    // Optional: Add tracked 2D features to a simple 3D point cloud proxy
    // Full triangulation requires tracking features across keyframes, building P1 and P2
    // For this lightweight version, we project the 2D pixel to 3D given the current depth estimate
    if (absolute_scale > 0.05 && m_mapPoints.size() < 2000) {
        for (size_t i = 0; i < goodCurr.size(); i++) {
            if (mask.at<uchar>(i) && rand() % 10 == 0) {
                // Approximate unprojection from pixel to 3D camera coordinates
                double x = (goodCurr[i].x - m_K.at<double>(0, 2)) / m_K.at<double>(0, 0);
                double y = (goodCurr[i].y - m_K.at<double>(1, 2)) / m_K.at<double>(1, 1);
                double z = 2.0; // Assume arbitrary depth for visual effect since full triangulation is omitted here
                
                cv::Mat pt3d_cam = (cv::Mat_<double>(3, 1) << x * z, y * z, z);
                cv::Mat pt3d_world = m_R_f.t() * (pt3d_cam - m_t_f);
                
                m_mapPoints.push_back(Eigen::Vector3f(
                    pt3d_world.at<double>(0),
                    pt3d_world.at<double>(1),
                    pt3d_world.at<double>(2)
                ));
            }
        }
    }

    // Prepare for next frame
    m_prevPoints = goodCurr;
    m_prevGray = gray.clone();
}

Eigen::Matrix4f SLAMEngine::getPose() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentPose;
}

std::vector<Eigen::Vector3f> SLAMEngine::getMapPoints() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_mapPoints;
}
