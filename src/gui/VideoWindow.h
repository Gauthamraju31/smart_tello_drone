#pragma once

#include "../core/VideoDecoder.h"
#include <imgui.h>

class VideoWindow {
public:
    VideoWindow(VideoDecoder& decoder);
    
    // Renders the video panel inside ImGui
    void render(bool* p_open = nullptr);

private:
    VideoDecoder& m_decoder;
};
