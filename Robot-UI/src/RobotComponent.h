
#pragma once

#include "Robot_API/robot_api.h"
#include <imgui.h>
#include <cstring>
#include <string>
#include <vector>

class RobotComponent
{
public:
    int  id = 0;
    bool isSelected = false;
    RobotMode component;

    void DrawConfigPanel();
};

// ============================================================================
// RobotNode — 纯数据（不含行为），用于序列化/传递
// ============================================================================
struct RobotNode
{
    int        id         = 0;
    bool       isSelected = false;
    RobotMode  component;
};
