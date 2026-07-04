#pragma once

#include "core.h"
#include "Robot_API/robot_api.h"
#include "Robot_API/hardware_interface.h"
#include "RobotCommManager.h"
#include "NodeGraphManager.h"
#include "LiveStreamManager.h"
#include "NodeGraph.h"

// ============================================================================
// ConnectionEntry — 单个机器人连接条目
// ============================================================================
struct ConnectionEntry
{
    std::shared_ptr<HardwareInterface> hw;
    RobotComm config;
    bool isLinked = false;
};

// ============================================================================
// ConnectionSnapshot — 线程安全的连接快照（供 GamepadRoutine 使用）
// ============================================================================
struct ConnectionSnapshot
{
    std::shared_ptr<HardwareInterface> hw;
    RobotComm config;
    bool isLinked = false;
    int commIndex = 0;  // 在 m_ActiveCommIndices 中的索引
};

// ============================================================================
// RobotStatus — 运行时状态监控 + 连接控制
// 持有 const RobotMode* 指针指向 RobotComponent 中的活跃模式（不持有拷贝）
// 支持多机器人广播：一个节点图 → 求值一次 → 发送到所有已连接的机器人
// ============================================================================

class RobotStatus
{
public:
    RobotStatus();
    ~RobotStatus();

    // ---- 活跃模式管理 ----
    void               SetActiveMode(const RobotComponent& comp);
    void               SetActiveGamepad(GamepadMapper* gp);
    // 加载当前活跃模式的节点图（根据 gamepadModeName 查找 node_graph_pairs）
    void               LoadGraph(const std::string& gamepadModeName);
    // ---- 节点图求值（线程安全） ----
    // 兼容旧 API：求值写入单个 ActuatorConfig
    void               EvaluateIntoActuator(const std::map<std::string, float>& keyValues, ActuatorConfig& data);
    // 新 API：按 CommIndex 分组求值到多个 ActuatorConfig
    void               EvaluateIntoActuators(const std::map<std::string, float>& keyValues,
                                             std::vector<ActuatorConfig>& dataVec,
                                             std::set<int>* pWrittenIndices = nullptr);
    bool               HasGraphEvaluator()    const;
    bool               HasActiveMode()        const;
    const RobotMode*     GetActiveModePtr()   const;
    GamepadMapper*       GetActiveGamepadPtr() const;
    std::string          GetActiveModeName()  const;

    // ---- 活跃模式配置快捷访问 ----
    const ActuatorConfig&        GetAppliedActuator()     const;
    const std::vector<ProtocolSendConfig>& GetAppliedSendConfig()   const;
    const std::vector<ProtocolReceiveConfig>& GetAppliedRecvConfig()   const;
    const SensorConfig&          GetSensorConfig()        const;
    bool HasTemperature()  const;
    bool HasHumidity()     const;
    bool HasDepth()        const;

    // ---- 快捷键：切换发送帧开关 ----
    void ToggleSendFrame(int index);        // 翻转发送帧 enabled
    void ToggleAllSendFrames();             // 全开/全关
    void OneShotSendFrame(int index);       // 只发一帧（短暂启用后立即关闭）

    // ---- 运行时数据更新 ----
    void UpdateCommandData(int index, std::shared_ptr<const ActuatorConfig> cmd);
    void UpdateAllCommandData(const std::vector<std::shared_ptr<const ActuatorConfig>>& cmds);
    void UpdateSensorData(const SensorData& sensor, bool valid);

    // ---- 运行时数据访问 ----
    std::shared_ptr<const ActuatorConfig> GetCurrentCommand(int index = 0) const;
    SensorData  GetCurrentSensor() const;
    bool        IsSensorValid()    const;

    // ---- 多连接（由 NodeGraph 的 CommRefs 决定，不手动管理）----
    int  GetConnectionCount() const;
    const ConnectionEntry* GetConnection(int index) const;
    bool IsLinked() const;
    bool LinkConnection(int index);
    void UnlinkConnection(int index);
    void UnlinkAll();

    // ---- 线程安全快照（供 GamepadRoutine 等外部线程使用） ----
    std::vector<ConnectionSnapshot> SnapshotConnections() const;

    // ---- 数据收发 ----
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

    // ---- 向求值器注入回调（供 ShortcutTrigger 节点使用） ----
    void SetEvaluatorSendActionCb(std::function<void(int,bool,bool)> cb);
    void SetEvaluatorShortcutManager(ShortcutManager* sm);

    // ---- 设置 Manager 引用（供 GamepadRoutine 独立同步图数据） ----
    void SetNodeGraphManager(NodeGraphManager* mgr) { m_NodeGraphManager = mgr; }

    // ---- 全局连接重试次数 (由 OptionPanel 设置) ----
    void SetConnRetryCount(int n) { if (n >= 1 && n <= 20) m_ConnRetryCount = n; }
    int  GetConnRetryCount() const { return m_ConnRetryCount; }
    void SetCameraRetryCount(int n) { if (n >= 0 && n <= 10) m_CameraRetryCount = n; }
    int  GetCameraRetryCount() const { return m_CameraRetryCount; }

    // ---- 从编辑器图同步节点/连线/CommRefs 到内部求值器 ----
    void SyncGraphFromEditor(NodeGraph* editor);

private:
    RobotMode                                  m_ActiveMode;
    bool                                       m_HasActiveMode = false;
    GamepadMapper*                              m_ActiveGamepad = nullptr;
    std::unique_ptr<NodeGraph>                  m_GraphEvaluator;
    std::string                                 m_LastSyncedYaml;  // skip re-sync if unchanged
    std::vector<std::shared_ptr<const ActuatorConfig>>  m_CurrentCommands;
    SensorData                                  m_CurrentSensor;
    bool                                        m_SensorValid   = false;

    // RobotStatus 自己的 Active 选择（不碰 Manager 的 Select）
    int m_ActiveLiveStreamIdx = 0;
    int m_ActiveNodeGraphIdx  = 0;
    std::vector<int> m_ActiveCommIndices;  // 来自 NodeGraph comm_refs
    bool m_DidFirstDerive = false;         // 首帧完成推导标志

public:
    // 调试：供 RobotSettingPanel 保存/恢复 active 状态
    int  GetActiveLiveStreamIdx() const { return m_ActiveLiveStreamIdx; }
    int  GetActiveNodeGraphIdx()  const { return m_ActiveNodeGraphIdx; }
    const std::vector<int>& GetActiveCommIndices() const { return m_ActiveCommIndices; }
    void SetActiveLiveStreamIdx(int i) { m_ActiveLiveStreamIdx = i; }
    void SetActiveNodeGraphIdx(int i)  { m_ActiveNodeGraphIdx  = i; }
    void SetActiveCommIndices(const std::vector<int>& v) { m_ActiveCommIndices = v; }

private:

    // 多连接池（RobotStatus 独占）—— 方案 B：一个节点图 → 广播到所有已连接机器人
    std::vector<ConnectionEntry>        m_Connections;
    mutable std::shared_mutex           m_ConnMutex;  // 保护 m_Connections 的并发访问

    // 外部 Manager 引用（用于同步 active 选择到实际行为）
    NodeGraphManager*       m_NodeGraphManager        = nullptr;
    RobotCommManager*       m_RobotCommManager        = nullptr;
    RobotComponentManager*  m_RobotComponentManager   = nullptr;
    GamepadMapperManager*   m_GamepadMapperManager    = nullptr;

    // 标记：下次 DrawWindow 时需要同步 NodeGraph
    bool m_NeedsNodeGraphSync = true;  // 初始为 true，首次 Draw 时同步

    // 跟踪上次同步协议配置的连接集合，切换时自动推送
    std::string m_LastSyncedProtocolKey;

    // 自动从 NodeGraph CommRefs 同步连接池
    void SyncConnectionsFromGraph();

    // live sync：RobotSettingPanel 打开时，evaluator 实时跟随 Manager 选中图
    bool m_LiveSyncToManager = false;

    int  m_ConnRetryCount = 6;   // 全局机器人连接重试次数（1~20）
    int  m_CameraRetryCount = 2; // 全局摄像头连接重试次数（额外次数，0~10）

    mutable std::shared_mutex                   m_StatusMutex;
};
