#pragma once

#include "imgui.h"
#include <functional>
#include <string>
#include <vector>

class RobotStatus;
class RobotSettingPanel;
class FileManager;

// ============================================================================
// 兼容 ImGui 1.84 — 自实现 ImGuiInputFlags (对齐 ImGui 1.91+ 路由语义)
// ============================================================================

enum ImGuiInputFlags_
{
    ImGuiInputFlags_None                = 0,
    ImGuiInputFlags_Repeat              = 1 << 0,
    ImGuiInputFlags_RouteActiveItem     = 1 << 1,
    ImGuiInputFlags_RouteGlobal         = 1 << 2,
    ImGuiInputFlags_RouteAlways         = 1 << 3,
    ImGuiInputFlags_RouteMask = ImGuiInputFlags_RouteActiveItem | ImGuiInputFlags_RouteGlobal | ImGuiInputFlags_RouteAlways,
};
typedef int ImGuiInputFlags;

// ============================================================================
// ShortcutBinding — 一个可重新绑定的快捷键
// ============================================================================
struct ShortcutBinding
{
    ImGuiKey key   = ImGuiKey_None;
    bool     ctrl  = false;
    bool     shift = false;
    bool     alt   = false;

    bool IsValid() const { return key != ImGuiKey_None; }

    // 格式化显示名称（如 "Ctrl + Shift + A"）
    static std::string ToString(ImGuiKey k, bool c, bool s, bool a);
    std::string ToString() const { return ToString(key, ctrl, shift, alt); }
};

// ============================================================================
// ShortcutManager — 全局快捷键路由 & 可重新绑定
// ============================================================================

class ShortcutManager
{
public:
    // ---- 动作 ID 枚举 ----
    enum Action : int {
        ACT_FILE_OPEN = 0,
        ACT_FILE_SAVE,
        ACT_FILE_SAVEAS,
        ACT_TOGGLE_OPTION,
        ACT_TOGGLE_STATUS,
        ACT_TOGGLE_ROBOTSETTING,
        ACT_TOGGLE_TERMINAL,
        ACT_TOGGLE_MONITORWALL,
        ACT_TOGGLE_THRUSTCURVE,
        ACT_TOGGLE_ABOUT,
        ACT_SCREENSHOT,
        ACT_COUNT
    };

    // ---- 依赖注入 ----
    void SetRobotStatus(RobotStatus* rs)           { m_RobotStatus = rs; }
    void SetRobotSettingPanel(RobotSettingPanel* rsp) { m_RobotSettingPanel = rsp; }
    void SetFileManager(FileManager* fm)            { m_FileManager = fm; }
    void SetFileCallbacks(std::function<void()> open, std::function<void()> save, std::function<void()> saveAs)
    {
        m_FileOpenCb  = std::move(open);
        m_FileSaveCb  = std::move(save);
        m_FileSaveAsCb = std::move(saveAs);
    }
    void SetScreenshotCallback(std::function<void()> cb) { m_ScreenshotCb = std::move(cb); }

    // ---- 绑定访问 ----
    ShortcutBinding&       GetBinding(int action)       { return m_Bindings[action]; }
    const ShortcutBinding& GetBinding(int action) const { return m_Bindings[action]; }
    int GetActionCount() const { return ACT_COUNT; }

    // ---- 静态工具 ----
    static bool Shortcut(ImGuiKey key, ImGuiInputFlags flags = 0, ImGuiID ownerId = 0);
    static std::string KeyName(ImGuiKey key);

    // ---- 可绑定按键列表（供录制/重绑定使用）----
    static const std::vector<ImGuiKey>& GetBindableKeys();

    // ---- 获取指定动作的默认绑定 ----
    static ShortcutBinding GetDefaultBinding(int action);

    // ---- 初始化面板开关指针 ----
    void InitPanelRefs(bool* optionOpen, bool* statusOpen, bool* robotSettingOpen, bool* terminalOpen,
                        bool* monitorWallOpen, bool* thrustCurveOpen, bool* aboutOpen);

    // ---- 每帧主入口（键盘快捷键）----
    void Process();

    // ---- 供 NodeGraph ShortcutTrigger 节点调用的动作执行 ----
    void ExecuteAction(int action);

    // ---- 动作描述（供 UI） ----
    static const char* GetActionCategory(int action);
    static const char* GetActionLabel(int action);

    // 序列化：软件 UI 快捷键 → .kernel
    std::string GetSoftwareBindingsYaml() const;
    void LoadSoftwareBindingsFromYaml(const std::string& yaml);

private:
    void InitDefaultBindings();

    RobotStatus*        m_RobotStatus        = nullptr;
    RobotSettingPanel*  m_RobotSettingPanel  = nullptr;
    FileManager*        m_FileManager        = nullptr;
    std::function<void()> m_FileOpenCb;
    std::function<void()> m_FileSaveCb;
    std::function<void()> m_FileSaveAsCb;
    std::function<void()> m_ScreenshotCb;

    bool* m_pOptionOpen        = nullptr;
    bool* m_pStatusOpen        = nullptr;
    bool* m_pRobotSettingOpen  = nullptr;
    bool* m_pTerminalOpen      = nullptr;
    bool* m_pMonitorWallOpen   = nullptr;
    bool* m_pThrustCurveOpen   = nullptr;
    bool* m_pAboutOpen         = nullptr;

    ShortcutBinding m_Bindings[ACT_COUNT];
    int  m_FrameCount = 0;
    bool m_WasActive[ACT_COUNT] = {}; // 上一帧按键是否激活（边沿检测）
};


