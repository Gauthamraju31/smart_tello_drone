#include "LogTerminal.h"

LogTerminal::LogTerminal(std::shared_ptr<ImGuiSink_mt> sink) 
    : m_sink(sink), m_autoScroll(true) {}

void LogTerminal::render(bool* p_open) {
    if (!ImGui::Begin("Log Terminal", p_open)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Clear")) m_sink->clear();
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);
    ImGui::Separator();

    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    auto msgs = m_sink->getMessages();
    
    // ImGuiListClipper handles large lists efficiently
    ImGuiListClipper clipper;
    clipper.Begin((int)msgs.size());
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            const auto& msg = msgs[i];
            
            ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Default White/Debug
            if (msg.level == spdlog::level::info) color = ImVec4(0.4f, 0.9f, 0.4f, 1.0f); // Green
            else if (msg.level == spdlog::level::warn) color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); // Yellow
            else if (msg.level >= spdlog::level::err) color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); // Red
            
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(msg.text.c_str());
            ImGui::PopStyleColor();
        }
    }
    clipper.End();

    if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::End();
}
