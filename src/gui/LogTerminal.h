#pragma once

#include "../utils/Logger.h"
#include <imgui.h>
#include <memory>

class LogTerminal {
public:
    LogTerminal(std::shared_ptr<ImGuiSink_mt> sink);
    
    void render(bool* p_open = nullptr);

private:
    std::shared_ptr<ImGuiSink_mt> m_sink;
    bool m_autoScroll;
};
