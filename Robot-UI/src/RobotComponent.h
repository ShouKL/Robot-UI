
#pragma once

#include "core.h"
#include "Robot_API/robot_api.h"

class RobotComponent
{
public:
    int  id = 0;
    bool isSelected = false;

    // ---- Direct fields ----
    char             name[64] = "";
    ActuatorConfig   actuator_config;
    SensorConfig     sensor_config;
};
