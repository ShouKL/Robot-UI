
#pragma once

#include "EditDraftBase.h"
#include "ShortcutManager.h"
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

    // ---- 供 Robot_UI_Layer 注入 ShortcutManager 引用 ----
    ShortcutManager* GetShortcutManager() { return m_ShortcutMgr; }
    void SetShortcutManager(ShortcutManager* sm) { m_ShortcutMgr = sm; }

private:
    void TakeSnapshots();
    void DrawShortcutsPanel();

    std::unique_ptr<ImGuiStyleManager> m_ImGuiStyleManager;
    ShortcutManager* m_ShortcutMgr = nullptr;

    int  m_SelectedId = 0;  // 0=Shortcuts, 1=Style
    bool m_OpenRobotSettingRequested = false;

    // 样式快照
    ImGuiTheme  m_StyleSnapshot_Theme  = ImGuiTheme::WalnutDefault;
    bool        m_StyleSnapshot_Invert  = false;
    float       m_StyleSnapshot_Alpha   = 1.0f;
};