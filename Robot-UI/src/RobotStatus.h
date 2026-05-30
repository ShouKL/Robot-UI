#pragma once

#include "Robot_API/robot_api.h"
#include "RobotCommManager.h"
#include "NodeGraph.h"
#include "Walnut/Core/Log.h"
#include <imgui.h>
#include <cstdlib>
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

// ============================================================================
// RobotStatus — 运行时状态监控
// 持有 const RobotMode* 指针指向 RobotComponent 中的活跃模式（不持有拷贝）
// ============================================================================

class RobotStatus
{
public:
    RobotStatus();

    // ---- 活跃模式管理 ----
    void               SetActiveMode(const RobotMode* mode);
    // 加载当前活跃模式的节点图（根据 gamepadModeName 查找 node_graph_pairs）
    void               LoadGraph(const std::string& gamepadModeName);
    // 节点图求值：key values → ActuatorConfig（线程安全，可在 GamepadRoutine 中调用）
    void               EvaluateIntoActuator(const std::map<std::string, float>& keyValues, ActuatorConfig& data);
    bool               HasGraphEvaluator()    const;
    bool               HasActiveMode()        const;
    const RobotMode*   GetActiveModePtr()     const;
    const std::string  GetActiveModeName()    const;

    // ---- 活跃模式配置快捷访问 ----
    const ActuatorConfig&        GetAppliedActuator()     const;
    const ProtocolSendConfig&    GetAppliedSendConfig()   const;
    const ProtocolReceiveConfig& GetAppliedRecvConfig()   const;
    const SensorConfig&          GetSensorConfig()        const;
    bool HasTemperature()  const;
    bool HasHumidity()     const;
    bool HasDepth()        const;

    // ---- 运行时数据更新 ----
    void UpdateCommandData(std::shared_ptr<const ActuatorConfig> cmd);
    void UpdateSensorData(const SensorData& sensor, bool valid);

    // ---- 运行时数据访问 ----
    std::shared_ptr<const ActuatorConfig> GetCurrentCommand() const;
    SensorData  GetCurrentSensor() const;
    bool        IsSensorValid()    const;

    // ---- UI ----
    void DrawWindow(bool* p_open, RobotCommManager* commManager = nullptr);

private:
    const RobotMode*                            m_ActiveMode    = nullptr;   // 指向 RobotComponent 中的模式
    std::unique_ptr<NodeGraph>                  m_GraphEvaluator;              // headless 节点图求值器
    std::shared_ptr<const ActuatorConfig>       m_CurrentCommand;
    SensorData                                  m_CurrentSensor;
    bool                                        m_SensorValid   = false;

    // 多线程保护：m_ActiveMode + m_GraphEvaluator 由 m_StatusMutex 保护
    // （GamepadRoutine 读，OnActiveModeChanged 回调写）
    mutable std::shared_mutex                   m_StatusMutex;
};
