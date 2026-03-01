#pragma once

#include <string>
#include <opencv2/core.hpp>

class FrameProcessor {
public:
    virtual ~FrameProcessor() = default;

    // Process a single frame. Should return the annotated frame (or unmodified if disabled)
    // This is called synchronously on the VideoDecoder's worker thread.
    virtual cv::Mat processFrame(const cv::Mat& frame) = 0;

    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;
    
    virtual std::string name() const = 0;

    // Optional initialization (e.g., loading ONNX models)
    virtual bool initialize() { return true; }
};
