
#pragma once

#include "EditDraftBase.h"
#include "ShortcutManager.h"
#include "imgui_style.h"

class MonitorWall;

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

    // ---- 供 Robot_UI_Layer 注入 MonitorWall 引用 ----
    void SetMonitorWall(MonitorWall* mw) { m_MonitorWall = mw; }

    // ---- 供 Robot_UI_Layer 注入 ShortcutManager 引用 ----
    ShortcutManager* GetShortcutManager() { return m_ShortcutMgr; }
    void SetShortcutManager(ShortcutManager* sm) { m_ShortcutMgr = sm; }

    // ---- 截图设置 getter/setter ----
    int  GetScreenshotScope() const { return m_ScreenshotScope; }
    void SetScreenshotScope(int scope) { m_ScreenshotScope = scope; }
    const std::string& GetScreenshotPath() const { return m_ScreenshotPath; }
    void SetScreenshotPath(const std::string& path) { m_ScreenshotPath = path; }

    // ---- 连接重试设置 getter/setter ----
    int  GetConnRetryCount() const { return m_ConnRetryCount; }
    void SetConnRetryCount(int n) { if (n >= 1 && n <= 20) m_ConnRetryCount = n; }
    int  GetCameraRetryCount() const { return m_CameraRetryCount; }
    void SetCameraRetryCount(int n) { if (n >= 0 && n <= 10) m_CameraRetryCount = n; }

private:
    void TakeSnapshots();
    void DrawShortcutsPanel();
    void DrawDisplayPanel();
    void DrawScreenshotPanel();
    void DrawConnectionPanel();

    std::unique_ptr<ImGuiStyleManager> m_ImGuiStyleManager;
    ShortcutManager* m_ShortcutMgr = nullptr;
    MonitorWall*     m_MonitorWall = nullptr;

    int  m_SelectedId = 0;  // 0=Shortcuts, 1=Style, 2=Screenshot
    int  m_RebindingAction = -1;         // 当前正在重绑定的动作 ID，-1 表示无
    bool m_RebindingWaitingRelease = true; // 等待所有按键释放后再开始捕获
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

    // 连接设置
    int         m_ConnRetryCount = 6;        // 机器人连接重试次数（1~20）
    int         m_ConnRetryCountSnapshot = 6;
    int         m_CameraRetryCount = 2;      // 摄像头连接额外重试次数（0~10）
    int         m_CameraRetryCountSnapshot = 2;
};