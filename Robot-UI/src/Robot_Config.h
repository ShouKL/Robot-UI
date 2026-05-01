#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Robot_API/robot_api.h"

#include <imgui.h>
#include <cstring>
#include <cstdint>
#include <iterator>

class Robot_Config {
public:
    Robot_Config();
    ~Robot_Config();

    bool DrawRobotConfigPanel();
    bool ApplyConfig();
    void RevertConfig();

    std::string host_ip = "192.168.0.10";
    int remote_port = 8888;
    int local_port = 8888;

    bool has_temperature = true;
    bool has_humidity = true;
    bool has_depth = true;

    ActuatorData actuator_config;

    std::string active_host_ip = "192.168.0.10";
    int active_remote_port = 8888;
    int active_local_port = 8888;

    bool active_has_temperature = true;
    bool active_has_humidity = true;
    bool active_has_depth = true;

    ActuatorData active_actuator_config;
};