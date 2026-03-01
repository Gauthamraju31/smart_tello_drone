#pragma once

#include "FrameProcessor.h"
#include <atomic>
#include <opencv2/dnn.hpp>
#include <vector>

class Segmentation : public FrameProcessor {
public:
    Segmentation();
    ~Segmentation() override = default;

    bool initialize() override;
    cv::Mat processFrame(const cv::Mat& frame) override;
    
    bool isEnabled() const override { return m_enabled.load(); }
    void setEnabled(bool enabled) override { m_enabled = enabled; }
    
    std::string name() const override { return "Segmentation (YOLOv11-seg)"; }

private:
    std::atomic<bool> m_enabled;
    cv::dnn::Net m_net;
    bool m_initialized = false;
    
    // Config params
    float m_confThreshold = 0.5f;
    float m_nmsThreshold = 0.4f;
    int m_inpWidth = 640;
    int m_inpHeight = 640;

    void postprocess(cv::Mat& frame, const std::vector<cv::Mat>& outs);
    
    std::vector<std::string> m_classes;
    std::vector<cv::Scalar> m_colors;
};
