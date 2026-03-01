#include "RecordingPanel.h"

RecordingPanel::RecordingPanel(Recorder& recorder, VideoDecoder& decoder) 
    : m_recorder(recorder), m_decoder(decoder) {}

void RecordingPanel::render(bool* p_open) {
    if (ImGui::Begin("Recording", p_open)) {
        
        bool isRec = m_recorder.isRecording();
        
        if (isRec) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("STOP RECORDING", ImVec2(-1, 50))) {
                m_recorder.stopRecording();
            }
            ImGui::PopStyleColor();
            
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Recording... %.1f s", m_recorder.getRecordingDuration());
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            if (ImGui::Button("START RECORDING", ImVec2(-1, 50))) {
                VideoFrame frame = m_decoder.getLatestFrameCopy();
                if (frame.width > 0 && frame.height > 0) {
                    m_recorder.startRecording(frame.width, frame.height, 30.0);
                }
            }
            ImGui::PopStyleColor();
        }

        ImGui::Separator();

        if (ImGui::Button("Take Snapshot", ImVec2(-1, 50))) {
            VideoFrame frame = m_decoder.getLatestFrameCopy();
            m_recorder.takeSnapshot(frame);
        }

    }
    ImGui::End();
}
