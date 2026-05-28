#pragma once

#include "Robot_API/robot_api.h"
#include "RobotComponent.h"
#include <memory>
#include <string>
#include "RobotCommManager.h"
#include "Walnut/Core/Log.h"
#include <imgui.h>
#include <map>
#include <vector>
#include <cstdlib>

class RobotStatus
{
public:
    RobotStatus();

    void ApplyFromRobotComponent(const RobotComponent& comp);

    void UpdateCommandData(std::shared_ptr<const ActuatorData> cmd);
    void UpdateSensorData(const SensorData& sensor, bool valid);

    void DrawWindow(bool* p_open, RobotCommManager* commManager = nullptr);

    const ActuatorData&          GetAppliedActuator()        const { return m_AppliedActuator; }
    const ProtocolSendConfig&    GetAppliedSendConfig()      const { return m_AppliedSendConfig; }
    const ProtocolReceiveConfig& GetAppliedRecvConfig()      const { return m_AppliedRecvConfig; }
    const std::string&           GetActiveModeName()         const { return m_ActiveModeName; }
    bool HasTemperature() const { return m_HasTemperature; }
    bool HasHumidity()    const { return m_HasHumidity; }
    bool HasDepth()       const { return m_HasDepth; }

    std::shared_ptr<const ActuatorData> GetCurrentCommand() const { return m_CurrentCommand; }
    const SensorData& GetCurrentSensor() const { return m_CurrentSensor; }
    bool IsSensorValid() const { return m_SensorValid; }

private:
    ActuatorData          m_AppliedActuator;
    ProtocolSendConfig    m_AppliedSendConfig;
    ProtocolReceiveConfig m_AppliedRecvConfig;
    std::string           m_ActiveModeName;
    bool m_HasTemperature = false;
    bool m_HasHumidity    = false;
    bool m_HasDepth       = false;

    std::shared_ptr<const ActuatorData> m_CurrentCommand;
    SensorData m_CurrentSensor;
    bool       m_SensorValid = false;
};
