#include "Theme.h"
#include <imgui.h>

void Theme::applyCatppuccinDark() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Catppuccin Mocha inspired palette
    const ImVec4 bg = ImColor(30, 30, 46);         // Base
    const ImVec4 surface0 = ImColor(49, 50, 68);     // Surface 0
    const ImVec4 surface1 = ImColor(69, 71, 90);     // Surface 1
    const ImVec4 surface2 = ImColor(88, 91, 112);    // Surface 2
    const ImVec4 text = ImColor(205, 214, 244);      // Text
    const ImVec4 textMuted = ImColor(166, 173, 200); // Subtext 0
    const ImVec4 accent = ImColor(137, 180, 250);    // Blue
    const ImVec4 accentHover = ImColor(180, 190, 254); // Lavender
    const ImVec4 accentActive = ImColor(116, 199, 236); // Sapphire
    const ImVec4 activeTab = ImColor(49, 50, 68);    // Match surface0

    colors[ImGuiCol_Text]                   = text;
    colors[ImGuiCol_TextDisabled]           = textMuted;
    colors[ImGuiCol_WindowBg]               = bg;
    colors[ImGuiCol_ChildBg]                = bg;
    colors[ImGuiCol_PopupBg]                = surface0;
    colors[ImGuiCol_Border]                 = surface1;
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = surface0;
    colors[ImGuiCol_FrameBgHovered]         = surface1;
    colors[ImGuiCol_FrameBgActive]          = surface2;
    colors[ImGuiCol_TitleBg]                = bg;
    colors[ImGuiCol_TitleBgActive]          = bg;
    colors[ImGuiCol_TitleBgCollapsed]       = bg;
    colors[ImGuiCol_MenuBarBg]              = surface0;
    colors[ImGuiCol_ScrollbarBg]            = bg;
    colors[ImGuiCol_ScrollbarGrab]          = surface1;
    colors[ImGuiCol_ScrollbarGrabHovered]   = surface2;
    colors[ImGuiCol_ScrollbarGrabActive]    = textMuted;
    colors[ImGuiCol_CheckMark]              = accent;
    colors[ImGuiCol_SliderGrab]             = accent;
    colors[ImGuiCol_SliderGrabActive]       = accentActive;
    colors[ImGuiCol_Button]                 = surface0;
    colors[ImGuiCol_ButtonHovered]          = surface1;
    colors[ImGuiCol_ButtonActive]           = surface2;
    colors[ImGuiCol_Header]                 = surface1;
    colors[ImGuiCol_HeaderHovered]          = surface2;
    colors[ImGuiCol_HeaderActive]           = accent;
    colors[ImGuiCol_Separator]              = surface1;
    colors[ImGuiCol_SeparatorHovered]       = surface2;
    colors[ImGuiCol_SeparatorActive]        = accent;
    colors[ImGuiCol_ResizeGrip]             = surface1;
    colors[ImGuiCol_ResizeGripHovered]      = surface2;
    colors[ImGuiCol_ResizeGripActive]       = accent;
    colors[ImGuiCol_Tab]                    = bg;
    colors[ImGuiCol_TabHovered]             = surface1;
    colors[ImGuiCol_TabActive]              = activeTab;
    colors[ImGuiCol_TabUnfocused]           = bg;
    colors[ImGuiCol_TabUnfocusedActive]     = activeTab;
    colors[ImGuiCol_DockingPreview]         = accentHover;
    colors[ImGuiCol_DockingEmptyBg]         = bg;

    // Styling
    style.WindowRounding = 6.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
}
