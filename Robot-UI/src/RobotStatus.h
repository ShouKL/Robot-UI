#pragma once

class GamepadMapper;
class GamepadMapperManager;
class LiveStreamManager;
class NodeGraphManager;
class RobotComponentManager;

#include "Robot_API/robot_api.h"
#include "Robot_API/hardware_interface.h"
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
// RobotStatus — 运行时状态监控 + 连接控制
// 持有 const RobotMode* 指针指向 RobotComponent 中的活跃模式（不持有拷贝）
// 负责 Link/Unlink，收发数据
// ============================================================================

class RobotStatus
{
public:
    RobotStatus();
    ~RobotStatus();

    // ---- 活跃模式管理 ----

    // ---- 活跃模式管理 ----
    void               SetActiveMode(const RobotMode* item);
    void               SetActiveGamepad(GamepadMapper* gp);
    // 加载当前活跃模式的节点图（根据 gamepadModeName 查找 node_graph_pairs）
    void               LoadGraph(const std::string& gamepadModeName);
    // 节点图求值：key values → ActuatorConfig（线程安全，可在 GamepadRoutine 中调用）
    void               EvaluateIntoActuator(const std::map<std::string, float>& keyValues, ActuatorConfig& data);
    bool               HasGraphEvaluator()    const;
    bool               HasActiveMode()        const;
    const RobotMode*     GetActiveModePtr()   const;
    GamepadMapper*       GetActiveGamepadPtr() const;
    const std::string    GetActiveModeName()  const;

    // ---- 活跃模式配置快捷访问 ----
    const ActuatorConfig&        GetAppliedActuator()     const;
    const std::vector<ProtocolSendConfig>& GetAppliedSendConfig()   const;
    const std::vector<ProtocolReceiveConfig>& GetAppliedRecvConfig()   const;
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

    // ---- 连接控制（RobotStatus 独占） ----
    bool Link(const RobotCommConfig& cfg);
    void Unlink();
    bool IsLinked() const { return m_IsLinked; }

    // ---- 数据收发（RobotStatus 独占） ----
    void       SendActuatorData(const ActuatorConfig& data);
    SensorData GetSensorData();

    // ---- UI ----
    void DrawWindow(bool* p_open, RobotCommManager* commManager = nullptr,
                    LiveStreamManager* liveStreamMgr = nullptr,
                    NodeGraphManager* nodeGraphMgr = nullptr,
                    RobotComponentManager* compMgr = nullptr,
                    GamepadMapperManager* gpMgr = nullptr);

    // ---- 同步 RobotStatus 的 active 选择到实际行为 ----
    void SyncActiveNodeGraph();
    void SyncFromManagerSelected();  // 直接从 NodeGraphManager 当前选中项同步
    void SyncFromManagerIfLive();    // 若 live sync 模式开启则实时同步
    void RequestNodeGraphSync() { m_NeedsNodeGraphSync = true; }
    void EnableLiveSync(bool enable);

    // ---- 获取当前活跃 Comm 的发送频率 ----
    int GetSendFreqHz() const;
    void DeriveActiveFromNodeGraph();

private:
    const RobotMode*                            m_ActiveMode    = nullptr;
    GamepadMapper*                              m_ActiveGamepad = nullptr;
    std::unique_ptr<NodeGraph>                  m_GraphEvaluator;
    std::string                                 m_LastSyncedYaml;  // skip re-sync if unchanged
    std::shared_ptr<const ActuatorConfig>       m_CurrentCommand;
    SensorData                                  m_CurrentSensor;
    bool                                        m_SensorValid   = false;

    // RobotStatus 自己的 Active 选择（不碰 Manager 的 Select）
    int m_ActiveLiveStreamIdx = 0;
    int m_ActiveNodeGraphIdx  = 0;
    int m_ActiveCommIdx       = 0;

public:
    // 调试：供 RobotSettingPanel 保存/恢复 active 状态
    int  GetActiveLiveStreamIdx() const { return m_ActiveLiveStreamIdx; }
    int  GetActiveNodeGraphIdx()  const { return m_ActiveNodeGraphIdx; }
    int  GetActiveCommIdx()       const { return m_ActiveCommIdx; }
    void SetActiveLiveStreamIdx(int i) { m_ActiveLiveStreamIdx = i; }
    void SetActiveNodeGraphIdx(int i)  { m_ActiveNodeGraphIdx  = i; }
    void SetActiveCommIdx(int i)       { m_ActiveCommIdx       = i; }

private:

    // 连接状态（RobotStatus 独占）
    std::shared_ptr<RobotAPI>           m_RobotAPI;
    bool                                m_IsLinked = false;

    // 外部 Manager 引用（用于同步 active 选择到实际行为）
    NodeGraphManager*       m_NodeGraphManager        = nullptr;
    RobotCommManager*       m_RobotCommManager        = nullptr;
    RobotComponentManager*  m_RobotComponentManager   = nullptr;
    GamepadMapperManager*   m_GamepadMapperManager    = nullptr;

    // 标记：下次 DrawWindow 时需要同步 NodeGraph
    bool m_NeedsNodeGraphSync = true;  // 初始为 true，首次 Draw 时同步

    // 跟踪上次同步协议配置的 item，切换时自动推送
    const RobotMode* m_LastSyncedProtocolItem = nullptr;

    // live sync：RobotSettingPanel 打开时，evaluator 实时跟随 Manager 选中图
    bool m_LiveSyncToManager = false;

    mutable std::shared_mutex                   m_StatusMutex;
};
