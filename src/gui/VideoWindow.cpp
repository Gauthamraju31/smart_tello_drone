#include "VideoWindow.h"
#include <algorithm>

VideoWindow::VideoWindow(VideoDecoder& decoder) : m_decoder(decoder) {}

void VideoWindow::render(bool* p_open) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); // No padding for video
    
    if (ImGui::Begin("Video Stream", p_open)) {
        // Grab the latest texture ID from the decoder (updated on main thread)
        GLuint texID = m_decoder.getLatestTextureID();
        
        if (texID != 0) {
            // Tello video is 960x720 (4:3)
            float videoAspect = 960.0f / 720.0f;
            
            ImVec2 avail = ImGui::GetContentRegionAvail();
            
            // Calculate size that preserves aspect ratio while fitting the window
            float targetWidth = avail.x;
            float targetHeight = targetWidth / videoAspect;
            
            if (targetHeight > avail.y) {
                targetHeight = avail.y;
                targetWidth = targetHeight * videoAspect;
            }
            
            // Center the image in the window
            ImVec2 cursor = ImGui::GetCursorPos();
            cursor.x += (avail.x - targetWidth) * 0.5f;
            cursor.y += (avail.y - targetHeight) * 0.5f;
            ImGui::SetCursorPos(cursor);
            
            // Render the OpenGL texture via ImGui
            // Cast GLuint to ImTextureID
            ImGui::Image((ImTextureID)(intptr_t)texID, ImVec2(targetWidth, targetHeight));
        } else {
            // Placeholder when no video is available
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImVec2 textStr = ImGui::CalcTextSize("No Video Stream");
            
            ImVec2 cursor = ImGui::GetCursorPos();
            cursor.x += (avail.x - textStr.x) * 0.5f;
            cursor.y += (avail.y - textStr.y) * 0.5f;
            ImGui::SetCursorPos(cursor);
            
            ImGui::TextDisabled("No Video Stream");
            
            ImGui::SetCursorPos(ImVec2(cursor.x, cursor.y + 20));
            ImVec2 textStr2 = ImGui::CalcTextSize("Connect to Drone -> Stream On");
            cursor.x = (avail.x - textStr2.x) * 0.5f;
            ImGui::SetCursorPos(ImVec2(cursor.x, cursor.y + 20));
            ImGui::TextDisabled("Connect to Drone -> Stream On");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
