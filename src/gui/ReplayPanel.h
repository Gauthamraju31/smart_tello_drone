#pragma once

#include <imgui.h>
#include "../core/ReplaySession.h"

class ReplayPanel {
public:
    ReplayPanel(ReplaySession& session);
    
    void render(bool* p_open = nullptr);

private:
    ReplaySession& m_session;
};
