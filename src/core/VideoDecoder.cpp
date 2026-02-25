#include "VideoDecoder.h"
#include <iostream>

extern "C" {
#include <libavutil/imgutils.h>
}

VideoDecoder::VideoDecoder() 
    : m_codec(nullptr), m_codecCtx(nullptr), m_parser(nullptr),
      m_packet(nullptr), m_frame(nullptr), m_swsCtx(nullptr),
      m_newFrameAvailable(false), m_textureID(0) {
}

VideoDecoder::~VideoDecoder() {
    shutdown();
}

bool VideoDecoder::initialize() {
    m_codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!m_codec) {
        std::cerr << "FFmpeg: H.264 decoder not found" << std::endl;
        return false;
    }

    m_codecCtx = avcodec_alloc_context3(m_codec);
    if (!m_codecCtx) {
        std::cerr << "FFmpeg: Could not allocate video codec context" << std::endl;
        return false;
    }

    m_parser = av_parser_init(m_codec->id);
    if (!m_parser) {
        std::cerr << "FFmpeg: Parser not found" << std::endl;
        return false;
    }

    // Tello stream is known low latency
    m_codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    m_codecCtx->thread_count = 2; // helps with 720p 

    if (avcodec_open2(m_codecCtx, m_codec, nullptr) < 0) {
        std::cerr << "FFmpeg: Could not open codec" << std::endl;
        return false;
    }

    m_packet = av_packet_alloc();
    m_frame = av_frame_alloc();

    if (!m_packet || !m_frame) {
        std::cerr << "FFmpeg: Could not allocate packet or frame" << std::endl;
        return false;
    }

    std::cout << "FFmpeg decoder initialized successfully" << std::endl;
    return true;
}

void VideoDecoder::shutdown() {
    if (m_parser) { av_parser_close(m_parser); m_parser = nullptr; }
    if (m_codecCtx) { avcodec_free_context(&m_codecCtx); }
    if (m_frame) { av_frame_free(&m_frame); }
    if (m_packet) { av_packet_free(&m_packet); }
    if (m_swsCtx) { sws_freeContext(m_swsCtx); m_swsCtx = nullptr; }

    if (m_textureID != 0) {
        glDeleteTextures(1, &m_textureID);
        m_textureID = 0;
    }
}

void VideoDecoder::decodeNetworkPacket(const uint8_t* data, size_t size) {
    if (!m_parser || !m_codecCtx) return;

    // We append the incoming UDP data to our parser buffer
    size_t data_pointer = 0;
    while (data_pointer < size) {
        int parsed = av_parser_parse2(m_parser, m_codecCtx, &m_packet->data, &m_packet->size,
                                      data + data_pointer, size - data_pointer,
                                      AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        if (parsed < 0) {
            std::cerr << "FFmpeg: Error parsing packet" << std::endl;
            return;
        }
        
        data_pointer += parsed;

        if (m_packet->size) {
            int ret = avcodec_send_packet(m_codecCtx, m_packet);
            if (ret < 0) {
                std::cerr << "FFmpeg: Error sending packet for decoding" << std::endl;
                continue;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(m_codecCtx, m_frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    std::cerr << "FFmpeg: Error during decoding" << std::endl;
                    break;
                }
                
                // Successfully decoded a frame (YUV420p)
                processDecodedFrame(m_frame);
            }
        }
    }
}

void VideoDecoder::processDecodedFrame(AVFrame* frame) {
    if (!m_swsCtx) {
        // Initialize swscale context on first frame or if size changes
        m_swsCtx = sws_getContext(frame->width, frame->height, (AVPixelFormat)frame->format,
                                  frame->width, frame->height, AV_PIX_FMT_RGB24,
                                  SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    }

    // Convert YUV to RGB
    VideoFrame rgbFrame;
    rgbFrame.width = frame->width;
    rgbFrame.height = frame->height;
    
    // allocate memory
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, frame->width, frame->height, 1);
    rgbFrame.data.resize(numBytes);

    uint8_t* dest_data[4] = { rgbFrame.data.data(), nullptr, nullptr, nullptr };
    int dest_linesize[4] = { rgbFrame.width * 3, 0, 0, 0 };

    sws_scale(m_swsCtx, frame->data, frame->linesize, 0, frame->height,
              dest_data, dest_linesize);

    // Call callback on worker thread before locking (for recording/AI hook)
    if (onFrameDecoded) {
        onFrameDecoded(rgbFrame);
    }

    {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        m_latestFrame = std::move(rgbFrame);
        m_newFrameAvailable = true;
    }
}

GLuint VideoDecoder::getLatestTextureID() {
    // This MUST be called from the main thread where the OpenGL context is active
    bool hasNewFrame = false;
    VideoFrame frame_copy;

    {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        if (m_newFrameAvailable) {
            frame_copy = m_latestFrame;
            hasNewFrame = true;
            m_newFrameAvailable = false;
        }
    }

    if (hasNewFrame) {
        uploadTexture(frame_copy);
    }

    return m_textureID;
}

VideoFrame VideoDecoder::getLatestFrameCopy() {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    return m_latestFrame; // Thread-safe copy
}

void VideoDecoder::uploadTexture(const VideoFrame& frame) {
    if (m_textureID == 0) {
        glGenTextures(1, &m_textureID);
    }

    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Upload RGB data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frame.width, frame.height, 0, GL_RGB, GL_UNSIGNED_BYTE, frame.data.data());
    glBindTexture(GL_TEXTURE_2D, 0); // Unbind
}

void VideoDecoder::setLatestFrame(const VideoFrame& frame) {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    m_latestFrame = frame;
    m_newFrameAvailable = true;
}
