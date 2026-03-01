#include "ObjectTracker.h"
#include <opencv2/imgproc.hpp>
#include <iostream>

ObjectTracker::ObjectTracker() : m_enabled(false) {}

bool ObjectTracker::initialize() {
    // TODO: Load ONNX model using cv::dnn::readNetFromONNX("yolov8n.onnx")
    std::cout << "ObjectTracker initialized (stub)" << std::endl;
    return true;
}

cv::Mat ObjectTracker::processFrame(const cv::Mat& frame) {
    if (!m_enabled || frame.empty()) return frame;

    // Clone since we might be drawing on it
    cv::Mat annotated = frame.clone();

    // --- PLACEHOLDER ---
    // In a real implementation, you would:
    // 1. Convert frame to blob: cv::dnn::blobFromImage()
    // 2. Set input: m_net.setInput(blob)
    // 3. Forward pass: std::vector<cv::Mat> outs; m_net.forward(outs, outNames)
    // 4. Run NMS on detections
    // 5. Update SORT/DeepSORT tracker with detections
    // 6. Draw bounding boxes + IDs

    // Draw a placeholder box in the center to prove the pipeline works
    int cx = frame.cols / 2;
    int cy = frame.rows / 2;
    int w = 150, h = 150;
    
    cv::Rect box(cx - w/2, cy - h/2, w, h);
    cv::rectangle(annotated, box, cv::Scalar(0, 255, 0), 2);
    cv::putText(annotated, "Tracking: None (Stub)", cv::Point(box.x, box.y - 10),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);

    return annotated;
}
