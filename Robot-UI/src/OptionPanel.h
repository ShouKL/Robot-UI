
#pragma once

#include "EditDraftBase.h"
#include "imgui_style.h"
#include <memory>

class OptionPanel : public EditDraftBase
{
public:
    OptionPanel();
    ~OptionPanel();

    void BeginEdit() override;
    void ApplyEdit() override;
    void CancelEdit() override;

    ImGuiStyleManager& GetImGuiStyleManager() { return *m_ImGuiStyleManager; }

    void DrawOptionPanel(bool* p_open);

    // 用户点击 "Open Robot Setting" 按钮时设置，供 Robot_UI 轮询
    bool IsRobotSettingRequested() const;
    void ClearRobotSettingRequest();

private:
    void TakeSnapshots();

    std::unique_ptr<ImGuiStyleManager> m_ImGuiStyleManager;

    int  m_SelectedId = 2;  // 0=Component, 1=GamepadMapper, 2=Style
    bool m_OpenRobotSettingRequested = false;

    ImGuiTheme  m_StyleSnapshot_Theme  = ImGuiTheme::WalnutDefault;
    bool        m_StyleSnapshot_Invert  = false;
    float       m_StyleSnapshot_Alpha   = 1.0f;
};