#include "ShortcutManager.h"

// ============================================================================
// Shortcut() — 兼容 ImGui 1.84 的路由式快捷键检测
// ============================================================================

bool ShortcutManager::Shortcut(ImGuiKey key, ImGuiInputFlags flags, ImGuiID ownerId)
{
    ImGuiIO& io = ImGui::GetIO();

    if (flags & ImGuiInputFlags_RouteActiveItem)
    {
        ImGuiContext* g = ImGui::GetCurrentContext();
        if (g && (ownerId == 0 || g->ActiveId != ownerId))
            return false;
        bool repeat = (flags & ImGuiInputFlags_Repeat) != 0;
        return ImGui::IsKeyPressed(key, repeat);
    }

    if (flags & ImGuiInputFlags_RouteGlobal)
    {
        if (io.WantTextInput) return false;
        bool repeat = (flags & ImGuiInputFlags_Repeat) != 0;
        return ImGui::IsKeyPressed(key, repeat);
    }

    if (flags & ImGuiInputFlags_RouteAlways)
    {
        bool repeat = (flags & ImGuiInputFlags_Repeat) != 0;
        return ImGui::IsKeyPressed(key, repeat);
    }

    if (io.WantTextInput) return false;
    return ImGui::IsKeyPressed(key, false);
}

// ============================================================================
// KeyName / ToString
// ============================================================================

std::string ShortcutManager::KeyName(ImGuiKey key)
{
    const char* name = ImGui::GetKeyName(key);
    if (name && name[0]) return name;
    char buf[16];
    snprintf(buf, sizeof(buf), "Key_%d", (int)key);
    return buf;
}

std::string ShortcutBinding::ToString(ImGuiKey k, bool c, bool s, bool a)
{
    if (k == ImGuiKey_None) return "(click to bind)";
    std::string out;
    if (c) out += "Ctrl + ";
    if (s) out += "Shift + ";
    if (a) out += "Alt + ";
    const char* name = ImGui::GetKeyName(k);
    out += (name && name[0]) ? name : "?";
    return out;
}

// ============================================================================
// 可绑定按键列表（与 OptionPanel 录制逻辑共用）
// ============================================================================
const std::vector<ImGuiKey>& ShortcutManager::GetBindableKeys()
{
    static const std::vector<ImGuiKey> keys = {
        ImGuiKey_A, ImGuiKey_B, ImGuiKey_C, ImGuiKey_D, ImGuiKey_E, ImGuiKey_F, ImGuiKey_G,
        ImGuiKey_H, ImGuiKey_I, ImGuiKey_J, ImGuiKey_K, ImGuiKey_L, ImGuiKey_M, ImGuiKey_N,
        ImGuiKey_O, ImGuiKey_P, ImGuiKey_Q, ImGuiKey_R, ImGuiKey_S, ImGuiKey_T, ImGuiKey_U,
        ImGuiKey_V, ImGuiKey_W, ImGuiKey_X, ImGuiKey_Y, ImGuiKey_Z,
        ImGuiKey_0, ImGuiKey_1, ImGuiKey_2, ImGuiKey_3, ImGuiKey_4,
        ImGuiKey_5, ImGuiKey_6, ImGuiKey_7, ImGuiKey_8, ImGuiKey_9,
        ImGuiKey_F1,  ImGuiKey_F2,  ImGuiKey_F3,  ImGuiKey_F4,
        ImGuiKey_F5,  ImGuiKey_F6,  ImGuiKey_F7,  ImGuiKey_F8,
        ImGuiKey_F9,  ImGuiKey_F10, ImGuiKey_F11, ImGuiKey_F12,
        ImGuiKey_GraveAccent, ImGuiKey_Space, ImGuiKey_Tab, ImGuiKey_Enter, ImGuiKey_Escape,
        ImGuiKey_Backspace, ImGuiKey_Delete,
        ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_UpArrow, ImGuiKey_DownArrow,
        ImGuiKey_PageUp, ImGuiKey_PageDown, ImGuiKey_Home, ImGuiKey_End,
        ImGuiKey_LeftCtrl, ImGuiKey_RightCtrl, ImGuiKey_LeftShift,
        ImGuiKey_RightShift, ImGuiKey_LeftAlt, ImGuiKey_RightAlt,
    };
    return keys;
}

// ============================================================================
// GetDefaultBinding — 返回指定动作的默认快捷键绑定
// ============================================================================
ShortcutBinding ShortcutManager::GetDefaultBinding(int action)
{
    static const ShortcutBinding defaults[] = {
        {ImGuiKey_N, true,  false, false},  // ACT_FILE_NEW             Ctrl+N
        {ImGuiKey_O, true,  false, false},  // ACT_FILE_OPEN            Ctrl+O
        {ImGuiKey_S, true,  false, false},  // ACT_FILE_SAVE            Ctrl+S
        {ImGuiKey_S, true,  true,  false},  // ACT_FILE_SAVEAS          Ctrl+Shift+S
        {ImGuiKey_F1, false, false, false},  // ACT_TOGGLE_OPTION        F1
        {ImGuiKey_F2, false, false, false},  // ACT_TOGGLE_STATUS        F2
        {ImGuiKey_F3, false, false, false},  // ACT_TOGGLE_ROBOTSETTING  F3
        {ImGuiKey_F4, false, false, false},  // ACT_TOGGLE_TERMINAL      F4
        {ImGuiKey_F5, false, false, false},  // ACT_TOGGLE_MONITORWALL   F5
        {ImGuiKey_F6, false, false, false},  // ACT_TOGGLE_THRUSTCURVE   F6
        {ImGuiKey_F7, false, false, false},  // ACT_TOGGLE_ABOUT         F7
        {ImGuiKey_F12,false, false, false},  // ACT_SCREENSHOT           F12
    };
    if (action >= 0 && action < (int)(sizeof(defaults) / sizeof(defaults[0])))
        return defaults[action];
    return {};
}

// ============================================================================
// 默认绑定 & 动作描述
// ============================================================================

void ShortcutManager::InitDefaultBindings()
{
    for (int i = 0; i < ACT_COUNT; ++i)
        m_Bindings[i] = GetDefaultBinding(i);
}

const char* ShortcutManager::GetActionCategory(int action)
{
    switch (action) {
    case ACT_FILE_NEW:
    case ACT_FILE_OPEN:
    case ACT_FILE_SAVE:
    case ACT_FILE_SAVEAS:          return "File";
    case ACT_TOGGLE_OPTION:
    case ACT_TOGGLE_STATUS:
    case ACT_TOGGLE_ROBOTSETTING:
    case ACT_TOGGLE_TERMINAL:
    case ACT_TOGGLE_MONITORWALL:
    case ACT_TOGGLE_THRUSTCURVE:
    case ACT_TOGGLE_ABOUT:         return "Navigation";
    case ACT_SCREENSHOT:           return "Tool";
    default:                       return "";
    }
}

const char* ShortcutManager::GetActionLabel(int action)
{
    switch (action) {
    case ACT_FILE_NEW:             return "File New";
    case ACT_FILE_OPEN:            return "File Open";
    case ACT_FILE_SAVE:            return "File Save";
    case ACT_FILE_SAVEAS:          return "File Save As";
    case ACT_TOGGLE_OPTION:        return "Toggle Option Panel";
    case ACT_TOGGLE_STATUS:        return "Toggle Status Panel";
    case ACT_TOGGLE_ROBOTSETTING:  return "Toggle Robot Setting Panel";
    case ACT_TOGGLE_TERMINAL:      return "Toggle Terminal Panel";
    case ACT_TOGGLE_MONITORWALL:   return "Toggle Monitor Wall";
    case ACT_TOGGLE_THRUSTCURVE:   return "Toggle Thrust Curve Editor";
    case ACT_TOGGLE_ABOUT:         return "Toggle About";
    case ACT_SCREENSHOT:           return "Screenshot";
    default:                       return "";
    }
}

std::string ShortcutManager::GetShortcutHint(int action) const
{
    if (action < 0 || action >= ACT_COUNT) return "";
    const auto& b = m_Bindings[action];
    if (!b.IsValid()) return "";
    return b.ToString();
}

// ==================== 初始化 ====================

void ShortcutManager::InitPanelRefs(bool* optionOpen, bool* statusOpen, bool* robotSettingOpen, bool* terminalOpen,
                                    bool* monitorWallOpen, bool* thrustCurveOpen, bool* aboutOpen)
{
    m_pOptionOpen       = optionOpen;
    m_pStatusOpen       = statusOpen;
    m_pRobotSettingOpen = robotSettingOpen;
    m_pTerminalOpen     = terminalOpen;
    m_pMonitorWallOpen  = monitorWallOpen;
    m_pThrustCurveOpen  = thrustCurveOpen;
    m_pAboutOpen        = aboutOpen;

    InitDefaultBindings();
}

// ============================================================================
// Process() — 每帧从 m_Bindings 读取，全部使用 Shortcut() 路由
// ============================================================================

void ShortcutManager::Process()
{
    ++m_FrameCount;

    // 启动冷却：前 60 帧（约 1 秒）完全跳过所有快捷键处理
    if (m_FrameCount < 60) return;

    ImGuiIO& io = ImGui::GetIO();

    // 边沿检测 toggle：仅当按键从未激活→激活时才翻转面板
    // 避免 docking 分支下 Begin/End 对不齐导致 IsKeyPressed 误触发
    #define TOGGLE_EDGE(idx, ptr) \
        { auto& b = m_Bindings[idx]; \
          if (b.IsValid()) { \
            bool active = (io.KeyCtrl == b.ctrl && io.KeyShift == b.shift && io.KeyAlt == b.alt) \
                       && ImGui::IsKeyDown(b.key) && !io.WantTextInput; \
            if (active && !m_WasActive[idx]) { \
                if (ptr) *ptr = !*ptr; \
            } \
            m_WasActive[idx] = active; \
          } }

    // 一次性动作：用 IsKeyPressed（只触发一次）
    #define ONESHOT(idx, cb) \
        { auto& b = m_Bindings[idx]; \
          if (b.IsValid() && io.KeyCtrl == b.ctrl && io.KeyShift == b.shift && io.KeyAlt == b.alt) \
            if (ImGui::IsKeyPressed(b.key, false) && !io.WantTextInput) \
                { if (cb) cb(); } }

    ONESHOT(ACT_FILE_NEW,    m_FileNewCb)
    ONESHOT(ACT_FILE_OPEN,   m_FileOpenCb)
    ONESHOT(ACT_FILE_SAVE,   m_FileSaveCb)
    ONESHOT(ACT_FILE_SAVEAS, m_FileSaveAsCb)
    ONESHOT(ACT_SCREENSHOT,  m_ScreenshotCb)

    TOGGLE_EDGE(ACT_TOGGLE_OPTION,       m_pOptionOpen)
    TOGGLE_EDGE(ACT_TOGGLE_STATUS,       m_pStatusOpen)
    TOGGLE_EDGE(ACT_TOGGLE_ROBOTSETTING, m_pRobotSettingOpen)
    TOGGLE_EDGE(ACT_TOGGLE_TERMINAL,     m_pTerminalOpen)
    TOGGLE_EDGE(ACT_TOGGLE_MONITORWALL,  m_pMonitorWallOpen)
    TOGGLE_EDGE(ACT_TOGGLE_THRUSTCURVE,  m_pThrustCurveOpen)
    TOGGLE_EDGE(ACT_TOGGLE_ABOUT,        m_pAboutOpen)

    #undef TOGGLE_EDGE
    #undef ONESHOT
}

// ============================================================================
// ExecuteAction — 供 NodeGraph ShortcutTrigger 节点调用
// ============================================================================
void ShortcutManager::ExecuteAction(int action)
{
    switch (action) {
    case ACT_FILE_NEW:    if (m_FileNewCb)   m_FileNewCb();   break;
    case ACT_FILE_OPEN:   if (m_FileOpenCb)  m_FileOpenCb();  break;
    case ACT_FILE_SAVE:   if (m_FileSaveCb)  m_FileSaveCb();  break;
    case ACT_FILE_SAVEAS: if (m_FileSaveAsCb) m_FileSaveAsCb(); break;
    case ACT_TOGGLE_OPTION:       if (m_pOptionOpen)       *m_pOptionOpen       = !*m_pOptionOpen;       break;
    case ACT_TOGGLE_STATUS:       if (m_pStatusOpen)       *m_pStatusOpen       = !*m_pStatusOpen;       break;
    case ACT_TOGGLE_ROBOTSETTING: if (m_pRobotSettingOpen) *m_pRobotSettingOpen = !*m_pRobotSettingOpen; break;
    case ACT_TOGGLE_TERMINAL:     if (m_pTerminalOpen)     *m_pTerminalOpen     = !*m_pTerminalOpen;     break;
    case ACT_TOGGLE_MONITORWALL:  if (m_pMonitorWallOpen)  *m_pMonitorWallOpen  = !*m_pMonitorWallOpen;  break;
    case ACT_TOGGLE_THRUSTCURVE:  if (m_pThrustCurveOpen)  *m_pThrustCurveOpen  = !*m_pThrustCurveOpen;  break;
    case ACT_TOGGLE_ABOUT:        if (m_pAboutOpen)        *m_pAboutOpen        = !*m_pAboutOpen;        break;
    case ACT_SCREENSHOT:         if (m_ScreenshotCb)       m_ScreenshotCb();                              break;
    default: break;
    }
}

// ============================================================================
// 序列化：软件 UI 快捷键（写入 kernel）
// ============================================================================

static std::string BindingToYamlEntry(const char* key, const ShortcutBinding& b)
{
    if (!b.IsValid()) return "";
    char buf[256];
    snprintf(buf, sizeof(buf), "    %s: {key: %d, ctrl: %s, shift: %s, alt: %s}\n",
        key, (int)b.key,
        b.ctrl ? "true" : "false",
        b.shift ? "true" : "false",
        b.alt ? "true" : "false");
    return buf;
}

std::string ShortcutManager::GetSoftwareBindingsYaml() const
{
    // 序列化所有软件 UI 快捷键
    std::string out;
    for (int i = ACT_FILE_OPEN; i < ACT_COUNT; ++i) {
        std::string entry = BindingToYamlEntry(GetActionLabel(i), m_Bindings[i]);
        out += entry;
    }
    return out;
}

void ShortcutManager::LoadSoftwareBindingsFromYaml(const std::string& yaml)
{
    YAML::Node root = YAML::Load(yaml);
    if (!root.IsMap()) return;
    for (int i = ACT_FILE_OPEN; i < ACT_COUNT; ++i) {
        const char* label = GetActionLabel(i);
        if (!label || !label[0]) continue;
        if (const YAML::Node& n = root[label]; n.IsDefined() && n.IsMap()) {
            if (const YAML::Node& k = n["key"]; k.IsDefined())
                m_Bindings[i].key = (ImGuiKey)k.as<int>();
            if (const YAML::Node& c = n["ctrl"]; c.IsDefined())
                m_Bindings[i].ctrl = c.as<bool>();
            if (const YAML::Node& s = n["shift"]; s.IsDefined())
                m_Bindings[i].shift = s.as<bool>();
            if (const YAML::Node& a = n["alt"]; a.IsDefined())
                m_Bindings[i].alt = a.as<bool>();
        }
    }
}
