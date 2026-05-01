#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstring>
#include "Walnut/Image.h"

struct KeyInfo {
    ImVec2 pos;       // 按键坐标
    ImVec2 textPos;   // 功能按钮显示的坐标
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

class GamepadMapper {
public:
    GamepadMapper();
    ~GamepadMapper();

    void DrawGamepadMapper();
    void UpdateGamepadState();
    float GetActionValue(const std::string& actionName);

    void SetMappings(const std::vector<KeyMapping>& mappings);
    void ApplyMappings();
    void RevertMappings();

    const std::vector<KeyMapping>& GetMappings() const { return m_Mappings; }

private:
    std::shared_ptr<Walnut::Image> m_GamepadImage;
    // UI 编辑用的状态
    std::vector<KeyMapping> m_Mappings;
    std::map<std::string, std::vector<std::string>> m_KeyBoundActions;

    // 实际生效的状态
    std::vector<KeyMapping> m_ActiveMappings;
    std::map<std::string, std::vector<std::string>> m_ActiveKeyBoundActions;

    std::vector<KeyInfo> m_Keys;

    std::string m_SelectedAction;
    std::map<std::string, float> m_ButtonIntensity;

    GLFWgamepadstate m_LastState;
    float m_LX = 0, m_LY = 0, m_RX = 0, m_RY = 0;

    static constexpr float k_BindThreshold = 0.5f;
    static constexpr float k_VisualDeadzone = 0.15f;
    static constexpr float k_StickVisualRange = 30.0f;

    void BindAction();
    void UnbindAction(const std::string& actionName);
    float CalculateIntensity(const KeyInfo& key, const GLFWgamepadstate& state);
};