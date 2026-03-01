#include "Segmentation.h"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <spdlog/spdlog.h>
#include <random>

Segmentation::Segmentation() : m_enabled(false) {
    // Generate colors for 80 COCO classes
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 255);
    for (int i = 0; i < 80; i++) {
        m_colors.push_back(cv::Scalar(dist(rng), dist(rng), dist(rng)));
    }
}

bool Segmentation::initialize() {
    try {
        m_net = cv::dnn::readNetFromONNX("model/yolo11n-seg.onnx");
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        m_initialized = true;
        spdlog::info("YOLOv11-seg initialized successfully.");
    } catch (const std::exception& e) {
        spdlog::warn("Failed to load yolo11n-seg.onnx: {}. Segmentation will be disabled.", e.what());
        m_initialized = false;
    }
    return m_initialized;
}

cv::Mat Segmentation::processFrame(const cv::Mat& frame) {
    if (!m_enabled || frame.empty() || !m_initialized) return frame;

    cv::Mat annotated = frame.clone();
    
    cv::Mat blob;
    cv::dnn::blobFromImage(frame, blob, 1.0/255.0, cv::Size(m_inpWidth, m_inpHeight), cv::Scalar(), true, false);
    m_net.setInput(blob);
    
    std::vector<cv::Mat> outs;
    std::vector<cv::String> names = m_net.getUnconnectedOutLayersNames();
    m_net.forward(outs, names);
    
    postprocess(annotated, outs);
    
    return annotated;
}

void Segmentation::postprocess(cv::Mat& frame, const std::vector<cv::Mat>& outs) {
    cv::Mat output0, output1;
    // Figure out which is boxes (dims=3) and which is masks (dims=4)
    if (outs[0].dims == 3) {
        output0 = outs[0];
        output1 = outs[1];
    } else {
        output0 = outs[1];
        output1 = outs[0];
    }
    
    // output0 is [1, 116, 8400]
    cv::Mat output0_2d(output0.size[1], output0.size[2], CV_32F, output0.ptr<float>());
    output0_2d = output0_2d.t(); // Transpose to [8400, 116]

    int num_proposals = output0_2d.rows;
    int num_classes = 80;
    int mask_coeffs = 32;

    std::vector<int> classIds;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;
    std::vector<std::vector<float>> masks;

    float x_factor = frame.cols / (float)m_inpWidth;
    float y_factor = frame.rows / (float)m_inpHeight;

    for (int i = 0; i < num_proposals; ++i) {
        float* row = output0_2d.ptr<float>(i);
        float* classes_scores = row + 4;
        
        cv::Mat scores(1, num_classes, CV_32F, classes_scores);
        cv::Point class_id;
        double max_class_score;
        cv::minMaxLoc(scores, 0, &max_class_score, 0, &class_id);
        
        if (max_class_score > m_confThreshold) {
            float cx = row[0];
            float cy = row[1];
            float w = row[2];
            float h = row[3];
            
            int left = int((cx - 0.5 * w) * x_factor);
            int top = int((cy - 0.5 * h) * y_factor);
            int width = int(w * x_factor);
            int height = int(h * y_factor);
            
            classIds.push_back(class_id.x);
            confidences.push_back((float)max_class_score);
            boxes.push_back(cv::Rect(left, top, width, height));
            
            std::vector<float> mask_coeff(row + 4 + num_classes, row + 4 + num_classes + mask_coeffs);
            masks.push_back(mask_coeff);
        }
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, m_confThreshold, m_nmsThreshold, indices);

    // output1 is [1, 32, 160, 160] -> [32, 25600]
    cv::Mat prototypes(32, 160 * 160, CV_32F, output1.ptr<float>());

    for (int idx : indices) {
        cv::Rect box = boxes[idx];
        int classId = classIds[idx];
        std::vector<float> mask_c = masks[idx];
        
        // Clamp box to frame
        box.x = std::max(0, box.x);
        box.y = std::max(0, box.y);
        box.width = std::min(frame.cols - box.x, box.width);
        box.height = std::min(frame.rows - box.y, box.height);
        if (box.width <= 0 || box.height <= 0) continue;

        cv::Mat mask_coeffs_mat(1, 32, CV_32F, mask_c.data());
        cv::Mat instance_mask = mask_coeffs_mat * prototypes;
        
        instance_mask = instance_mask.reshape(1, 160); // [160, 160]
        
        // Sigmoid
        cv::exp(-instance_mask, instance_mask);
        instance_mask = 1.0f / (1.0f + instance_mask);

        // Resize to network input shape [640, 640]
        cv::Mat resized_mask;
        cv::resize(instance_mask, resized_mask, cv::Size(m_inpWidth, m_inpHeight), 0, 0, cv::INTER_LINEAR);
        
        // Crop resized mask exactly to the original frame box (taking x/y factors into account)
        cv::Rect mask_box(
            int(box.x / x_factor), 
            int(box.y / y_factor), 
            int(box.width / x_factor), 
            int(box.height / y_factor)
        );
        
        mask_box.x = std::max(0, mask_box.x);
        mask_box.y = std::max(0, mask_box.y);
        mask_box.width = std::min(m_inpWidth - mask_box.x, mask_box.width);
        mask_box.height = std::min(m_inpHeight - mask_box.y, mask_box.height);
        if (mask_box.width <= 0 || mask_box.height <= 0) continue;

        cv::Mat roi_mask = resized_mask(mask_box);
        cv::resize(roi_mask, roi_mask, box.size(), 0, 0, cv::INTER_LINEAR);
        
        roi_mask = roi_mask > 0.5f;

        // Draw bounding box
        cv::Scalar color = m_colors[classId % 80];
        cv::rectangle(frame, box, color, 2);
        
        std::string label = cv::format("Class %d: %.2f", classId, confidences[idx]);
        cv::putText(frame, label, cv::Point(box.x, box.y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);

        // Alpha blend the mask
        cv::Mat roi = frame(box);
        cv::Mat colorMask = cv::Mat(roi.size(), CV_8UC3, color);
        
        std::vector<cv::Mat> channels(3);
        cv::split(roi, channels);
        for (int c = 0; c < 3; ++c) {
            channels[c].setTo(color[c], roi_mask);
        }
        cv::Mat blended;
        cv::merge(channels, blended);
        cv::addWeighted(roi, 0.6, blended, 0.4, 0, roi);
    }
}
