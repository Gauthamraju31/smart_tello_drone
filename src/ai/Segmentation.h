#pragma once

#include "FrameProcessor.h"
#include <atomic>

class Segmentation : public FrameProcessor {
public:
    Segmentation();
    ~Segmentation() override = default;

    bool initialize() override;
    cv::Mat processFrame(const cv::Mat& frame) override;
    
    bool isEnabled() const override { return m_enabled.load(); }
    void setEnabled(bool enabled) override { m_enabled = enabled; }
    
    std::string name() const override { return "Segmentation (SAM)"; }

private:
    std::atomic<bool> m_enabled;
    // cv::dnn::Net m_net; // Future SAM or FastSAM ONNX model
};
