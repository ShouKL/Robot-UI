#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstring>
#include "Walnut/Image.h"

struct KeyInfo {
    ImVec2 pos;
    ImVec2 textPos;
    float radius;
    std::string name;
    std::string label;
    int glfw_id;
    bool is_axis;
};

struct KeyMapping {
    std::string action_name;
    std::string gamepad_key;
    ImVec2 key_pos;
    ImVec2 action_pos;
    bool is_bound;
    bool is_analog;
};

struct GamepadMode {
    char name[64] = "Default";
    std::vector<KeyMapping> mappings;
};

class GamepadMapper {
public:
    GamepadMapper();
    ~GamepadMapper();

    void DrawGamepadMapper();
    void UpdateGamepadState();

    float GetActionValue(const std::string& actionName);

    // 模式管理
    std::vector<std::string> GetModeNames() const;
    int  GetActiveModeIndex() const { return m_ActiveModeIndex; }

    void SetActiveMode(const std::string& name);
    void SetActiveModeByIndex(int index);
    const std::string& GetActiveModeName() const { return m_Modes[m_ActiveModeIndex].name; }

    void AddMode();
    void DeleteMode(int index);
    bool ModeExists(const std::string& name) const;

    // 草稿机制
    void BeginEdit(int modeIndex);
    void ApplyEdit();
    void CancelEdit();
    bool IsEditing() const { return m_IsEditing; }

    // 获取模式列表（只读）及 UI 模式选择索引
    const std::vector<GamepadMode>& GetModes() const { return m_Modes; }
    int& GetEditModeIndexRef() { return m_SelectedModeIndex; }   // 外部用于同步侧边栏选中

private:
    std::shared_ptr<Walnut::Image> m_GamepadImage;

    std::vector<GamepadMode> m_Modes;
    int m_ActiveModeIndex = 0;                // 控制线程使用的模式
    int m_SelectedModeIndex = 0;              // UI 侧边栏当前选中的编辑目标模式索引

    // 编辑草稿
    GamepadMode  m_EditingMode;
    bool         m_IsEditing = false;
    int          m_EditingModeIndex = -1;

    mutable std::mutex m_ModeMutex;

    std::map<std::string, std::vector<std::string>> m_KeyBoundActions;
    std::string m_SelectedAction;

    std::vector<KeyInfo> m_Keys;
    std::map<std::string, float> m_ButtonIntensity;

    GLFWgamepadstate m_LastState;
    float m_LX = 0, m_LY = 0, m_RX = 0, m_RY = 0;

    static constexpr float k_BindThreshold = 0.5f;
    static constexpr float k_VisualDeadzone = 0.15f;

    void BindAction();
    void UnbindAction(const std::string& actionName);
    float CalculateIntensity(const KeyInfo& key, const GLFWgamepadstate& state);
    void RefreshBoundActions();

    const GamepadMode& GetDisplayMode() const;
    GamepadMode& GetDisplayMode();
};