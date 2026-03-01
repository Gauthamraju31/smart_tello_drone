#pragma once

#include <vector>
#include <mutex>
#include <functional>
#include <atomic>
#include <thread>
#include <cstdint>
#include <GL/gl.h>
#include <memory>
#include "../ai/FrameProcessor.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

// Simple structure to hold RGB frame data
struct VideoFrame {
    std::vector<uint8_t> data;
    int width = 0;
    int height = 0;
    int64_t timestamp_ms = 0;
};

class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();

    bool initialize();
    void shutdown();

    // Push raw H.264 NAL units from TelloSDK
    void decodeNetworkPacket(const uint8_t* data, size_t size);

    // Call this from the main ImGui render thread to get the latest texture ID
    // Returns 0 if no texture is available yet
    GLuint getLatestTextureID();

    VideoFrame getLatestFrameCopy(); // Thread-safe copy for AI processing or recording

    // Callback fired when a new frame is decoded (from worker thread)
    std::function<void(const VideoFrame&)> onFrameDecoded;

    void uploadTexture(const VideoFrame& frame); // Must be called on main thread context
    
    // Allows external sources (like ReplaySession) to safely inject a frame
    void setLatestFrame(const VideoFrame& frame);

    void addProcessor(std::shared_ptr<FrameProcessor> processor) {
        m_processors.push_back(processor);
    }

private:
    void processDecodedFrame(AVFrame* frame);

    const AVCodec* m_codec;
    AVCodecContext* m_codecCtx;
    AVCodecParserContext* m_parser;
    AVPacket* m_packet;
    AVFrame* m_frame;
    SwsContext* m_swsCtx;

    std::mutex m_frameMutex;
    VideoFrame m_latestFrame;    // Raw RGB data
    bool m_newFrameAvailable;    // Flag for main thread
    
    GLuint m_textureID;          // OpenGL texture ID

    std::vector<uint8_t> m_buffer;
    std::vector<std::shared_ptr<FrameProcessor>> m_processors;
};
