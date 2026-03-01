#pragma once

#include "FrameProcessor.h"
#include <atomic>

class ObjectTracker : public FrameProcessor {
public:
    ObjectTracker();
    ~ObjectTracker() override = default;

    bool initialize() override;
    cv::Mat processFrame(const cv::Mat& frame) override;
    
    bool isEnabled() const override { return m_enabled.load(); }
    void setEnabled(bool enabled) override { m_enabled = enabled; }
    
    std::string name() const override { return "Object Tracker (YOLO)"; }

private:
    std::atomic<bool> m_enabled;
    // cv::dnn::Net m_net; // Future YOLOv8 ONNX model
};
