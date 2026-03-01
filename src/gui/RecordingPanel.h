#pragma once

#include "../core/Recorder.h"
#include <imgui.h>

class RecordingPanel {
public:
    RecordingPanel(Recorder& recorder, VideoDecoder& decoder);
    
    void render(bool* p_open = nullptr);

private:
    Recorder& m_recorder;
    VideoDecoder& m_decoder;
};
