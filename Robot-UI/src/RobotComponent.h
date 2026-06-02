
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
