#include "imgui_style.h"

ImGuiStyleManager& ImGuiStyleManager::WalnutDefaultTheme()

{

    ImGui::GetStyle() = m_DefaultStyle;

    return *this;

}

ImGuiStyleManager& ImGuiStyleManager::CinderTheme()

{

    mStyle.WindowMinSize = ImVec2(160, 20);

    mStyle.FramePadding = ImVec2(4, 2);

    mStyle.ItemSpacing = ImVec2(6, 2);

    mStyle.ItemInnerSpacing = ImVec2(6, 4);

    mStyle.Alpha = 0.95f;

    mStyle.WindowRounding = 4.0f;

    mStyle.FrameRounding = 2.0f;

    mStyle.IndentSpacing = 6.0f;

    mStyle.ItemInnerSpacing = ImVec2(2, 4);

    mStyle.ColumnsMinSpacing = 50.0f;

    mStyle.GrabMinSize = 14.0f;

    mStyle.GrabRounding = 16.0f;

    mStyle.ScrollbarSize = 12.0f;

    mStyle.ScrollbarRounding = 16.0f;

    mStyle.Colors[ImGuiCol_Text] = ImVec4(0.86f, 0.93f, 0.89f, 0.78f);

    mStyle.Colors[ImGuiCol_TextDisabled] = ImVec4(0.86f, 0.93f, 0.89f, 0.28f);

    mStyle.Colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);

    mStyle.Colors[ImGuiCol_Border] = ImVec4(0.31f, 0.31f, 1.00f, 0.00f);

    mStyle.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    mStyle.Colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);

    mStyle.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);

    mStyle.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);

    mStyle.Colors[ImGuiCol_TitleBg] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);

    mStyle.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.20f, 0.22f, 0.27f, 0.75f);

    mStyle.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);

    mStyle.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.20f, 0.22f, 0.27f, 0.47f);

    mStyle.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);

    mStyle.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.09f, 0.15f, 0.16f, 1.00f);

    mStyle.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);

    mStyle.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);

    mStyle.Colors[ImGuiCol_CheckMark] = ImVec4(0.71f, 0.22f, 0.27f, 1.00f);

    mStyle.Colors[ImGuiCol_SliderGrab] = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);

    mStyle.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);

    mStyle.Colors[ImGuiCol_Button] = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);

    mStyle.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);

    mStyle.Colors[ImGuiCol_ButtonActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);

    mStyle.Colors[ImGuiCol_Header] = ImVec4(0.92f, 0.18f, 0.29f, 0.76f);

    mStyle.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);

    mStyle.Colors[ImGuiCol_HeaderActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);

    mStyle.Colors[ImGuiCol_Separator] = ImVec4(0.14f, 0.16f, 0.19f, 1.00f);

    mStyle.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);

    mStyle.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);

    mStyle.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.47f, 0.77f, 0.83f, 0.04f);

    mStyle.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);

    mStyle.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);

    mStyle.Colors[ImGuiCol_PlotLines] = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);

    mStyle.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);

    mStyle.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);

    mStyle.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);

    mStyle.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.92f, 0.18f, 0.29f, 0.43f);

    mStyle.Colors[ImGuiCol_PopupBg] = ImVec4(0.20f, 0.22f, 0.27f, 0.9f);

    mStyle.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.22f, 0.27f, 0.73f);

    return *this;

}

ImGuiStyleManager& ImGuiStyleManager::SpectrumTheme()

{

    mStyle.GrabRounding = 4.0f;

    ImVec4* colors = mStyle.Colors;

    colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY800); // text on hovered controls is gray900

    colors[ImGuiCol_TextDisabled] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY500);

    colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY100);

    colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    colors[ImGuiCol_PopupBg] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY50); // not sure about this. Note: applies to tooltips too.

    colors[ImGuiCol_Border] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY300);

    colors[ImGuiCol_BorderShadow] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::Static::NONE); // We don't want shadows. Ever.

    colors[ImGuiCol_FrameBg] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY75); // this isnt right, spectrum does not do this, but it's a good fallback

    colors[ImGuiCol_FrameBgHovered] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY50);

    colors[ImGuiCol_FrameBgActive] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY200);

    colors[ImGuiCol_TitleBg] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY300); // those titlebar values are totally made up, spectrum does not have this.

    colors[ImGuiCol_TitleBgActive] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY200);

    colors[ImGuiCol_TitleBgCollapsed] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY400);

    colors[ImGuiCol_MenuBarBg] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY100);

    colors[ImGuiCol_ScrollbarBg] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY100); // same as regular background

    colors[ImGuiCol_ScrollbarGrab] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY400);

    colors[ImGuiCol_ScrollbarGrabHovered] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY600);

    colors[ImGuiCol_ScrollbarGrabActive] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY700);

    colors[ImGuiCol_CheckMark] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::BLUE500);

    colors[ImGuiCol_SliderGrab] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY700);

    colors[ImGuiCol_SliderGrabActive] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY800);

    colors[ImGuiCol_Button] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY75); // match default button to Spectrum's 'Action Button'.

    colors[ImGuiCol_ButtonHovered] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY50);

    colors[ImGuiCol_ButtonActive] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY200);

    colors[ImGuiCol_Header] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::BLUE400);

    colors[ImGuiCol_HeaderHovered] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::BLUE500);

    colors[ImGuiCol_HeaderActive] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::BLUE600);

    colors[ImGuiCol_Separator] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY400);

    colors[ImGuiCol_SeparatorHovered] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY600);

    colors[ImGuiCol_SeparatorActive] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY700);

    colors[ImGuiCol_ResizeGrip] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY400);

    colors[ImGuiCol_ResizeGripHovered] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY600);

    colors[ImGuiCol_ResizeGripActive] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::GRAY700);

    colors[ImGuiCol_PlotLines] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::BLUE400);

    colors[ImGuiCol_PlotLinesHovered] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::BLUE600);

    colors[ImGuiCol_PlotHistogram] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::BLUE400);

    colors[ImGuiCol_PlotHistogramHovered] = ImGui::ColorConvertU32ToFloat4(ImGui::Spectrum::BLUE600);

    colors[ImGuiCol_TextSelectedBg] = ImGui::ColorConvertU32ToFloat4((ImGui::Spectrum::BLUE400 & 0x00FFFFFF) | 0x33000000);

    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);

    colors[ImGuiCol_NavHighlight] = ImGui::ColorConvertU32ToFloat4((ImGui::Spectrum::GRAY900 & 0x00FFFFFF) | 0x0A000000);

    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);

    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);

    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

    return *this;

}

ImGuiStyleManager& ImGuiStyleManager::EnemymouseTheme()

{

    mStyle.Alpha = 1.0;

/*      mStyle.WindowFillAlphaDefault = 0.83;*/   // done

    mStyle.ChildRounding = 3;

    mStyle.WindowRounding = 3;

    mStyle.GrabRounding = 1;

    mStyle.GrabMinSize = 20;

    mStyle.FrameRounding = 3;

    mStyle.Colors[ImGuiCol_Text] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);

    mStyle.Colors[ImGuiCol_TextDisabled] = ImVec4(0.00f, 0.40f, 0.41f, 1.00f);

    mStyle.Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);

    mStyle.Colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    mStyle.Colors[ImGuiCol_Border] = ImVec4(0.00f, 1.00f, 1.00f, 0.65f);

    mStyle.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    mStyle.Colors[ImGuiCol_FrameBg] = ImVec4(0.44f, 0.80f, 0.80f, 0.18f);

    mStyle.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.44f, 0.80f, 0.80f, 0.27f);

    mStyle.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.44f, 0.81f, 0.86f, 0.66f);

    mStyle.Colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.18f, 0.21f, 0.73f);

    mStyle.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.54f);

    mStyle.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.27f);

    mStyle.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);

    mStyle.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.22f, 0.29f, 0.30f, 0.71f);

    mStyle.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.00f, 1.00f, 1.00f, 0.44f);

    mStyle.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.74f);

    mStyle.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);

    mStyle.Colors[ImGuiCol_PopupBg] = ImVec4(0.16f, 0.24f, 0.22f, 0.60f);

    mStyle.Colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 1.00f, 1.00f, 0.68f);

    mStyle.Colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 1.00f, 1.00f, 0.36f);

    mStyle.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.76f);

    mStyle.Colors[ImGuiCol_Button] = ImVec4(0.00f, 0.65f, 0.65f, 0.46f);

    mStyle.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.01f, 1.00f, 1.00f, 0.43f);

    mStyle.Colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.62f);

    mStyle.Colors[ImGuiCol_Header] = ImVec4(0.00f, 1.00f, 1.00f, 0.33f);

    mStyle.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.42f);

    mStyle.Colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.54f);

    mStyle.Colors[ImGuiCol_Separator] = ImVec4(0.00f, 0.50f, 0.50f, 0.33f);

    mStyle.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.00f, 0.50f, 0.50f, 0.47f);

    mStyle.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.00f, 0.70f, 0.70f, 1.00f);

    mStyle.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 1.00f, 1.00f, 0.54f);

    mStyle.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.74f);

    mStyle.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);

    /*  mStyle.Colors[ImGuiCol_CloseButton] = ImVec4(0.00f, 0.78f, 0.78f, 0.35f);

    mStyle.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.00f, 0.78f, 0.78f, 0.47f);

    mStyle.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.00f, 0.78f, 0.78f, 1.00f);*/ // done

    mStyle.Colors[ImGuiCol_PlotLines] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);

    mStyle.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);

    mStyle.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);

    mStyle.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);

    mStyle.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 1.00f, 1.00f, 0.22f);

    mStyle.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.04f, 0.10f, 0.09f, 0.51f);

    return *this;

}

ImGuiStyleManager& ImGuiStyleManager::LedSynthmasterTheme()

{

    mStyle.WindowPadding = ImVec2(15, 15);

    mStyle.WindowRounding = 5.0f;

    mStyle.FramePadding = ImVec2(5, 5);

    mStyle.FrameRounding = 4.0f;

    mStyle.ItemSpacing = ImVec2(12, 8);

    mStyle.ItemInnerSpacing = ImVec2(8, 6);

    mStyle.IndentSpacing = 25.0f;

    mStyle.ScrollbarSize = 15.0f;

    mStyle.ScrollbarRounding = 9.0f;

    mStyle.GrabMinSize = 5.0f;

    mStyle.GrabRounding = 3.0f;

    mStyle.Colors[ImGuiCol_Text] = ImVec4(0.40f, 0.39f, 0.38f, 1.00f);

    mStyle.Colors[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.39f, 0.38f, 0.77f);

    mStyle.Colors[ImGuiCol_WindowBg] = ImVec4(0.92f, 0.91f, 0.88f, 0.70f);

    mStyle.Colors[ImGuiCol_ChildBg] = ImVec4(1.00f, 0.98f, 0.95f, 0.58f);

    mStyle.Colors[ImGuiCol_PopupBg] = ImVec4(0.92f, 0.91f, 0.88f, 0.92f);

    mStyle.Colors[ImGuiCol_Border] = ImVec4(0.84f, 0.83f, 0.80f, 0.65f);

    mStyle.Colors[ImGuiCol_BorderShadow] = ImVec4(0.92f, 0.91f, 0.88f, 0.00f);

    mStyle.Colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 0.98f, 0.95f, 1.00f);

    mStyle.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.99f, 1.00f, 0.40f, 0.78f);

    mStyle.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 1.00f, 0.00f, 1.00f);

    mStyle.Colors[ImGuiCol_TitleBg] = ImVec4(1.00f, 0.98f, 0.95f, 1.00f);

    mStyle.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 0.98f, 0.95f, 0.75f);

    mStyle.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);

    mStyle.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.00f, 0.98f, 0.95f, 0.47f);

    mStyle.Colors[ImGuiCol_ScrollbarBg] = ImVec4(1.00f, 0.98f, 0.95f, 1.00f);

    mStyle.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.00f, 0.00f, 0.00f, 0.21f);

    mStyle.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.90f, 0.91f, 0.00f, 0.78f);

    mStyle.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);

    mStyle.Colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 0.98f, 0.95f, 1.00f);

    mStyle.Colors[ImGuiCol_CheckMark] = ImVec4(0.25f, 1.00f, 0.00f, 0.80f);

    mStyle.Colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 0.00f, 0.00f, 0.14f);

    mStyle.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);

    mStyle.Colors[ImGuiCol_Button] = ImVec4(0.00f, 0.00f, 0.00f, 0.14f);

    mStyle.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.99f, 1.00f, 0.22f, 0.86f);

    mStyle.Colors[ImGuiCol_ButtonActive] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);

    mStyle.Colors[ImGuiCol_Header] = ImVec4(0.25f, 1.00f, 0.00f, 0.76f);

    mStyle.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 1.00f, 0.00f, 0.86f);

    mStyle.Colors[ImGuiCol_HeaderActive] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);

    mStyle.Colors[ImGuiCol_Separator] = ImVec4(0.00f, 0.00f, 0.00f, 0.32f);

    mStyle.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.25f, 1.00f, 0.00f, 0.78f);

    mStyle.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);

    mStyle.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.04f);

    mStyle.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.25f, 1.00f, 0.00f, 0.78f);

    mStyle.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);

    /*mStyle.Colors[ImGuiCol_CloseButton] = ImVec4(0.40f, 0.39f, 0.38f, 0.16f);

    mStyle.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.40f, 0.39f, 0.38f, 0.39f);

    mStyle.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.40f, 0.39f, 0.38f, 1.00f);*/ // done

    mStyle.Colors[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);

    mStyle.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);

    mStyle.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);

    mStyle.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);

    mStyle.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.25f, 1.00f, 0.00f, 0.43f);

    mStyle.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 0.98f, 0.95f, 0.73f);

    return *this;

}

ImGuiStyleManager& ImGuiStyleManager::DougblinksTheme()

{

    mStyle.Alpha = 1.0f;

    mStyle.FrameRounding = 3.0f;

    mStyle.Colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);

    mStyle.Colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);

    mStyle.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 0.94f);

    mStyle.Colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    mStyle.Colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);

    mStyle.Colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);

    mStyle.Colors[ImGuiCol_BorderShadow] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);

    mStyle.Colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);

    mStyle.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);

    mStyle.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);

    mStyle.Colors[ImGuiCol_TitleBg] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);

    mStyle.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);

    mStyle.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);

    mStyle.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);

    mStyle.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);

    mStyle.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);

    mStyle.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);

    mStyle.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);

    mStyle.Colors[ImGuiCol_PopupBg] = ImVec4(0.86f, 0.86f, 0.86f, 0.99f);

    mStyle.Colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

    mStyle.Colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);

    mStyle.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

    mStyle.Colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);

    mStyle.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

    mStyle.Colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);

    mStyle.Colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);

    mStyle.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);

    mStyle.Colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

    mStyle.Colors[ImGuiCol_Separator] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);

    mStyle.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.78f);

    mStyle.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

    mStyle.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.50f);

    mStyle.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);

    mStyle.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);

    /* mStyle.Colors[ImGuiCol_CloseButton] = ImVec4(0.59f, 0.59f, 0.59f, 0.50f);

        mStyle.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.98f, 0.39f, 0.36f, 1.00f);

        mStyle.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.98f, 0.39f, 0.36f, 1.00f);*/  // done

    mStyle.Colors[ImGuiCol_PlotLines] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);

    mStyle.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);

    mStyle.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);

    mStyle.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);

    mStyle.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);

    mStyle.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

    return *this;

}

ImGuiStyleManager& ImGuiStyleManager::ClassicTheme()

{

    ImGui::StyleColorsClassic();

    return *this;

}

ImGuiStyleManager& ImGuiStyleManager::DarkTheme()

{

    ImGui::StyleColorsDark();

    return *this;

}

ImGuiStyleManager& ImGuiStyleManager::LightTheme()

{

    ImGui::StyleColorsLight();

    return *this;

}

void ImGuiStyleManager::DrawStylePanel()

{

    if (ImGui::BeginTable("SettingsTable", 2)) {

        ImGui::TableNextColumn();

        ImGui::Text("Theme");

        ImGui::TableNextColumn();

        ImGui::SetNextItemWidth(-FLT_MIN);

        ImGui::Combo("##Theme", (int*)&m_Theme, themeNames, IM_ARRAYSIZE(themeNames));

        ImGui::TableNextColumn();

        ImGui::Text("Invert Mode");

        ImGui::TableNextColumn();

        ImGui::Checkbox("##InvertMode", &m_bStyleInvert);

        ImGui::TableNextColumn();

        ImGui::Text("Alpha");

        ImGui::TableNextColumn();

        ImGui::SetNextItemWidth(-FLT_MIN);

        ImGui::SliderFloat("##Alpha", &m_Alpha, 0.1f, 1.0f);

        ImGui::TableNextColumn();

        ImGui::Text("Preview");

        ImGui::TableNextColumn();

        if (m_PreviewTimer > 0.0f) {

            m_PreviewTimer -= ImGui::GetIO().DeltaTime;

            if (m_PreviewTimer <= 0.0f) {

                m_PreviewTimer = 0.0f;

                SetStyleColors(m_ActiveTheme, m_ActiveBStyleInvert, m_ActiveAlpha);

            }

            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Previewing... %.1fs", m_PreviewTimer);

        } else {

            if (ImGui::Button("Preview (3s)")) {

                m_PreviewTimer = 3.0f;

                SetStyleColors(m_Theme, m_bStyleInvert, m_Alpha);

            }

        }

        ImGui::EndTable();

    }

}

void ImGuiStyleManager::SetStyleColors(ImGuiTheme theme, bool bStyleInvert_, float alpha_)

{

    ImGuiStyle& style = ImGui::GetStyle();

    style = m_DefaultStyle; // /

    switch (theme) {

    case ImGuiTheme::WalnutDefault: { WalnutDefaultTheme(); break; }

    case ImGuiTheme::Cinder: { CinderTheme();         break; }

    case ImGuiTheme::AdobeSpectrum: { SpectrumTheme();       break; }

    case ImGuiTheme::EnemyMouse: { EnemymouseTheme();     break; }

    case ImGuiTheme::LedSynthmaster: { LedSynthmasterTheme(); break; }

    case ImGuiTheme::Dougblinks: { DougblinksTheme();     break; }

    case ImGuiTheme::Classic: { ClassicTheme();        break; }

    case ImGuiTheme::Dark: { DarkTheme();           break; }

    case ImGuiTheme::Light: { LightTheme();          break; }

    default: { ClassicTheme();        break; }

    }

    if (bStyleInvert_)

    {

        for (int i = 0; i < ImGuiCol_COUNT; i++)

        {

            ImVec4& col = style.Colors[i];

            float H, S, V;

            ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, H, S, V);

            if (S < 0.1f)

            {

                V = 1.0f - V;

            }

            ImGui::ColorConvertHSVtoRGB(H, S, V, col.x, col.y, col.z);

            if (col.w < 1.00f)

            {

                col.w *= alpha_;

            }

        }

    }

    else

    {

        for (int i = 0; i < ImGuiCol_COUNT; i++)

        {

            ImVec4& col = style.Colors[i];

            if (col.w < 1.00f)

            {

                col.x *= alpha_;

                col.y *= alpha_;

                col.z *= alpha_;

                col.w *= alpha_;

            }

        }

    }

}

void ImGuiStyleManager::ApplyImGuiStyle(ImGuiTheme theme, bool bStyleInvert_, float alpha_)

{

    m_Theme = theme;

    m_ActiveTheme = theme;

    m_bStyleInvert = bStyleInvert_;

    m_ActiveBStyleInvert = bStyleInvert_;

    m_Alpha = alpha_;

    m_ActiveAlpha = alpha_;

    m_PreviewTimer = 0.0f;

    SetStyleColors(theme, bStyleInvert_, alpha_);

}

void ImGuiStyleManager::ApplyActiveStyle()

{

    ApplyImGuiStyle(m_Theme, m_bStyleInvert, m_Alpha);

}

void ImGuiStyleManager::RevertStyle()

{

    m_Theme = m_ActiveTheme;

    m_bStyleInvert = m_ActiveBStyleInvert;

    m_Alpha = m_ActiveAlpha;

    if (m_PreviewTimer > 0.0f) {

        m_PreviewTimer = 0.0f;

        SetStyleColors(m_ActiveTheme, m_ActiveBStyleInvert, m_ActiveAlpha);

    }

}

ImGuiStyleManager::ImGuiStyleManager() : mStyle(ImGui::GetStyle()), mIO(ImGui::GetIO())

{

    m_DefaultStyle = ImGui::GetStyle();

}

ImGuiStyleManager::~ImGuiStyleManager()

{

}
