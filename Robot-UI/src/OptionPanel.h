#pragma once
#include <memory>
#include "GamepadMapper.h"
#include "imgui_style.h"
#include "Robot_Config.h"

class OptionPanel {
public:
    OptionPanel();
    ~OptionPanel();

    void DrawOptionPanel();

    bool Apply();       // 应用所有配置（Robot、Gamepad、Style）
    void Revert();      // 撤销所有修改

    std::unique_ptr<GamepadMapper>& GetGamepadMapper() { return m_GamepadMapper; }
    std::unique_ptr<Robot_Config>& GetRobotConfig() { return m_RobotConfig; }

private:
    std::unique_ptr<GamepadMapper> m_GamepadMapper;
    std::unique_ptr<ImGuiStyleManager> m_ImGuiStyleManager;
    std::unique_ptr<Robot_Config> m_RobotConfig;

    int selected_id = 0;                // 左侧大类：0=Robot, 1=Gamepad, 2=Style
    bool m_RobotConfigChanged = false;

    // 用于跟踪上一次在 GamepadMapper 页签中编辑的模式索引，避免重复 BeginEdit
    int m_LastGamepadEditIndex = -1;
    bool m_GamepadEditActive = false;   // 是否正处于手柄编辑状态
};