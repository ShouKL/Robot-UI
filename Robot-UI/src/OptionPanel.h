#pragma once
#include <string>
#include <vector>
#include "GamepadMapper.h"
#include "imgui_style.h"
#include "Robot_Config.h"

class OptionPanel {
public:
    OptionPanel();
    ~OptionPanel();
    void DrawOptionPanel();
    bool Apply();
    void Revert();

    std::unique_ptr<GamepadMapper>& GetGamepadMapper() { return m_GamepadMapper; }
    std::unique_ptr<Robot_Config>& GetRobotConfig() { return m_RobotConfig; }

private:
    std::unique_ptr<GamepadMapper> m_GamepadMapper;
    std::unique_ptr<ImGuiStyleManager> m_ImGuiStyleManager;
    std::unique_ptr<Robot_Config> m_RobotConfig;

    const char* items[3] = {  "Robot", "GamepadMapper", "Style" };
    int selected_id = 0;
    bool m_RobotConfigChanged = false;
};