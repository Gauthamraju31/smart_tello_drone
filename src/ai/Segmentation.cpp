#include "Segmentation.h"
#include <opencv2/imgproc.hpp>
#include <iostream>

Segmentation::Segmentation() : m_enabled(false) {}

bool Segmentation::initialize() {
    // TODO: Load ONNX model for SAM / Mask R-CNN
    std::cout << "Segmentation initialized (stub)" << std::endl;
    return true;
}

cv::Mat Segmentation::processFrame(const cv::Mat& frame) {
    if (!m_enabled || frame.empty()) return frame;

    cv::Mat annotated = frame.clone();

    // --- PLACEHOLDER ---
    // In a real implementation:
    // 1. Inference via ONNX
    // 2. Extract mask output
    // 3. Alpha blend mask over original frame
    
    // Draw dummy mask (semi-transparent red circle in center)
    cv::Mat overlay;
    frame.copyTo(overlay);
    
    cv::circle(overlay, cv::Point(frame.cols/2, frame.rows/2), 100, cv::Scalar(255, 0, 0), -1);
    
    // Blend: annotated = 0.5 * overlay + 0.5 * frame
    cv::addWeighted(overlay, 0.4, annotated, 0.6, 0, annotated);
    
    cv::putText(annotated, "Seg: Dummy Mask", cv::Point(20, 20),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);

    return annotated;
}
