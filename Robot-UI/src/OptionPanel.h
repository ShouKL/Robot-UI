
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

    // ---- 截图设置 getter/setter ----
    int  GetScreenshotScope() const { return m_ScreenshotScope; }
    void SetScreenshotScope(int scope) { m_ScreenshotScope = scope; }
    const std::string& GetScreenshotPath() const { return m_ScreenshotPath; }
    void SetScreenshotPath(const std::string& path) { m_ScreenshotPath = path; }

private:
    void TakeSnapshots();
    void DrawShortcutsPanel();
    void DrawScreenshotPanel();

    std::unique_ptr<ImGuiStyleManager> m_ImGuiStyleManager;
    ShortcutManager* m_ShortcutMgr = nullptr;

    int  m_SelectedId = 0;  // 0=Shortcuts, 1=Style, 2=Screenshot
    bool m_OpenRobotSettingRequested = false;

    // 样式快照
    ImGuiTheme  m_StyleSnapshot_Theme  = ImGuiTheme::WalnutDefault;
    bool        m_StyleSnapshot_Invert  = false;
    float       m_StyleSnapshot_Alpha   = 1.0f;

    // 截图设置
    int         m_ScreenshotScope = 0;      // 0=主窗口, 1=整个屏幕
    std::string m_ScreenshotPath;            // 保存路径
    int         m_ScreenshotScopeSnapshot = 0;
    std::string m_ScreenshotPathSnapshot;
};