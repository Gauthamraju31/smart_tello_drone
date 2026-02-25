#include "ReplayPanel.h"
#include <imgui.h>
#include <iomanip>
#include <sstream>

ReplayPanel::ReplayPanel(ReplaySession& session) : m_session(session) {}

// Helper to format ms into MM:SS
static std::string formatTime(int64_t ms) {
    int total_seconds = ms / 1000;
    int minutes = total_seconds / 60;
    int seconds = total_seconds % 60;
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << minutes << ":" << std::setfill('0') << std::setw(2) << seconds;
    return ss.str();
}

void ReplayPanel::render(bool* p_open) {
    if (ImGui::Begin("Replay Controls", p_open)) {
        if (!m_session.isRunning()) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Replay Session Not Active or Finished");
            ImGui::End();
            return;
        }

        bool isPaused = m_session.isPaused();
        
        // Play/Pause button
        if (isPaused) {
            if (ImGui::Button("Play", ImVec2(100, 30))) {
                m_session.setPaused(false);
            }
        } else {
            if (ImGui::Button("Pause", ImVec2(100, 30))) {
                m_session.setPaused(true);
            }
        }
        
        ImGui::SameLine();
        
        // Time text
        std::string currentTime = formatTime(m_session.getCurrentTimeMs());
        std::string totalTime = formatTime(m_session.getTotalDurationMs());
        ImGui::Text("%s / %s", currentTime.c_str(), totalTime.c_str());

        // Progress bar (Interactive Scrubber)
        float progress = m_session.getProgress();
        if (ImGui::SliderFloat("##progress", &progress, 0.0f, 1.0f, "")) {
            m_session.seek(progress);
        }
    }
    ImGui::End();
}
