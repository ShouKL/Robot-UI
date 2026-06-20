#include "RobotStatus.h"
#include "GamepadMapper.h"
#include "GamepadMapperManager.h"
#include "LiveStreamManager.h"
#include "NodeGraphManager.h"
#include "RobotCommManager.h"
#include "RobotComponentManager.h"
#include "Walnut/Core/Log.h"
#include <cstring>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

RobotStatus::RobotStatus()
{
    m_CurrentCommand = std::make_shared<const ActuatorConfig>();
    m_GraphEvaluator = std::make_unique<NodeGraph>();
    WL_INFO_TAG("ROBOT_STATUS", "RobotStatus created (multi-connection mode, with headless graph evaluator)");
}

// ---- 同步 ActiveNodeGraph 到求值器 ----
void RobotStatus::SyncActiveNodeGraph()
{
    if (!m_NodeGraphManager) return;
    if (m_ActiveNodeGraphIdx < 0 || m_ActiveNodeGraphIdx >= m_NodeGraphManager->GetItemCount()) return;

    std::string yaml = m_NodeGraphManager->GetGraphDataYamlForIndex(m_ActiveNodeGraphIdx);
    if (yaml.empty()) return;

    if (yaml == m_LastSyncedYaml) return;
    m_LastSyncedYaml = yaml;

    std::unique_lock<std::shared_mutex> lock(m_StatusMutex);
    if (m_GraphEvaluator->LoadGraphData(yaml)) {
        WL_INFO_TAG("ROBOT_STATUS", "Synced graph from active NodeGraph #{} ({})",
                    m_ActiveNodeGraphIdx, m_NodeGraphManager->GetItemNameBuf(m_ActiveNodeGraphIdx));
        m_ActiveCommIndices = m_GraphEvaluator->GetCommRefs();
    } else {
        WL_WARN_TAG("ROBOT_STATUS", "Failed to parse graph from active NodeGraph #{}",
                    m_ActiveNodeGraphIdx);
    }
}

void RobotStatus::EnableLiveSync(bool enable)
{
    m_LiveSyncToManager = enable;
    WL_INFO_TAG("ROBOT_STATUS", "Live sync to Manager: {}", enable ? "ON" : "OFF");
}

void RobotStatus::SyncFromManagerIfLive()
{
    if (!m_LiveSyncToManager || !m_NodeGraphManager) return;
    SyncFromManagerSelected();
}

void RobotStatus::SyncFromManagerSelected()
{
    if (!m_NodeGraphManager) return;
    int selIdx = m_NodeGraphManager->GetSelectedIndex();
    if (selIdx < 0 || selIdx >= m_NodeGraphManager->GetItemCount()) return;

    std::string yaml = m_NodeGraphManager->GetGraphYamlForIndex(selIdx);
    if (yaml.empty()) return;

    if (yaml == m_LastSyncedYaml) return;
    m_LastSyncedYaml = yaml;

    m_ActiveNodeGraphIdx = selIdx;

    std::unique_lock<std::shared_mutex> lock(m_StatusMutex);
    if (m_GraphEvaluator->LoadGraphData(yaml))
        WL_INFO_TAG("ROBOT_STATUS", "Synced graph from Manager selected #{} ({})",
                    selIdx, m_NodeGraphManager->GetItemNameBuf(selIdx));
    else
        WL_WARN_TAG("ROBOT_STATUS", "Failed to parse graph from Manager selected #{}", selIdx);
}

// ---- 获取当前活跃 Comm 的发送频率（取所有连接中的最小值）----
int RobotStatus::GetSendFreqHz() const
{
    int minHz = 200; // 安全上限
    for (const auto& conn : m_Connections) {
        if (!conn.isLinked) continue;
        int hz = conn.config.send_freq_hz;
        if (hz > 0 && hz < minHz) minHz = hz;
    }
    if (minHz == 200) {
        // 无已连接实例时，回退到活跃 Comm 配置（取最小值）
        if (!m_RobotCommManager || m_ActiveCommIndices.empty()) return 100;
        auto items = m_RobotCommManager->GetAllItems();
        for (int idx : m_ActiveCommIndices) {
            if (idx < 0 || idx >= (int)items.size()) continue;
            int hz = items[idx].send_freq_hz;
            if (hz > 0 && hz < minHz) minHz = hz;
        }
        return (minHz == 200) ? 100 : minHz;
    }
    return minHz;
}

// ---- 从当前选中的 NodeGraph 推导 ActiveMode / ActiveGamepad / ActiveComm ----
void RobotStatus::DeriveActiveFromNodeGraph()
{
    if (!m_NodeGraphManager) return;
    if (m_ActiveNodeGraphIdx < 0 || m_ActiveNodeGraphIdx >= m_NodeGraphManager->GetItemCount()) return;

    std::string yaml = m_NodeGraphManager->GetGraphYamlForIndex(m_ActiveNodeGraphIdx);
    if (yaml.empty()) return;

    try {
        YAML::Node root = YAML::Load(yaml);

        // 推导 Component（通过 active_robot_mode 名称匹配）
        if (m_RobotComponentManager && root["active_robot_mode"]) {
            std::string modeName = root["active_robot_mode"].as<std::string>();
            if (!modeName.empty()) {
                auto& comps = m_RobotComponentManager->GetComponents();
                for (auto& c : comps) {
                    if (std::strcmp(c.name, modeName.c_str()) == 0) {
                        SetActiveMode(c);
                        WL_INFO_TAG("ROBOT_STATUS", "Derived ActiveMode '{}' from NodeGraph #{}",
                                    modeName, m_ActiveNodeGraphIdx);
                        break;
                    }
                }
            }
        }

        // 推导 Gamepad（通过 active_gamepad_mode 名称匹配）
        if (m_GamepadMapperManager && root["active_gamepad_mode"]) {
            std::string gpName = root["active_gamepad_mode"].as<std::string>();
            if (!gpName.empty()) {
                auto& mappers = m_GamepadMapperManager->GetMappers();
                for (auto& gm : mappers) {
                    if (std::strcmp(gm.name, gpName.c_str()) == 0) {
                        SetActiveGamepad(&gm);
                        WL_INFO_TAG("ROBOT_STATUS", "Derived ActiveGamepad '{}' from NodeGraph #{}",
                                    gpName, m_ActiveNodeGraphIdx);
                        break;
                    }
                }
            }
        }

        // 推导 Comm（读 comm_refs 数组，回退到旧格式 comm_index）
        if (m_RobotCommManager) {
            int commMgrCount = m_RobotCommManager->GetItemCount();
            if (root["comm_refs"] && root["comm_refs"].IsSequence()) {
                m_ActiveCommIndices.clear();
                for (auto r : root["comm_refs"]) {
                    int ci = r.as<int>();
                    if (ci >= 0 && ci < commMgrCount)
                        m_ActiveCommIndices.push_back(ci);
                }
                WL_INFO_TAG("ROBOT_STATUS", "Derived {} ActiveComm(s) from NodeGraph #{} comm_refs",
                            m_ActiveCommIndices.size(), m_ActiveNodeGraphIdx);
            } else if (root["comm_index"]) {
                int commIdx = root["comm_index"].as<int>();
                m_ActiveCommIndices.clear();
                if (commIdx >= 0 && commIdx < commMgrCount) {
                    m_ActiveCommIndices.push_back(commIdx);
                    WL_INFO_TAG("ROBOT_STATUS", "Derived ActiveComm #{} from NodeGraph #{} (old comm_index)",
                                commIdx, m_ActiveNodeGraphIdx);
                }
            }
        }
    }
    catch (const std::exception& e) {
        WL_WARN_TAG("ROBOT_STATUS", "DeriveActiveFromNodeGraph YAML parse failed: {}", e.what());
    }
}

RobotStatus::~RobotStatus()
{
    UnlinkAll();
}

// ---- 向求值器注入回调（供 ShortcutTrigger 节点使用） ----
void RobotStatus::SetEvaluatorSendActionCb(std::function<void(int,bool,bool)> cb)
{
    std::unique_lock<std::shared_mutex> lock(m_StatusMutex);
    if (m_GraphEvaluator)
        m_GraphEvaluator->SetSendActionCb(std::move(cb));
}

void RobotStatus::SetEvaluatorShortcutManager(ShortcutManager* sm)
{
    std::unique_lock<std::shared_mutex> lock(m_StatusMutex);
    if (m_GraphEvaluator)
        m_GraphEvaluator->SetShortcutManager(sm);
}

// ---- 从编辑器图同步到内部求值器 ----
void RobotStatus::SyncGraphFromEditor(NodeGraph* editor)
{
    if (!editor || !m_GraphEvaluator) return;
    std::string yaml = editor->GetGraphDataYaml();
    if (yaml.empty()) return;

    std::unique_lock<std::shared_mutex> lock(m_StatusMutex);
    if (m_GraphEvaluator->LoadGraphData(yaml))
        WL_INFO_TAG("ROBOT_STATUS", "Synced graph from editor");
    else
        WL_WARN_TAG("ROBOT_STATUS", "Failed to sync graph from editor");
}

// ---- 活跃模式管理（写锁） ----

void RobotStatus::SetActiveMode(const RobotComponent& comp)
{
    std::unique_lock<std::shared_mutex> lock(m_StatusMutex);
    strncpy(m_ActiveMode.name, comp.name, sizeof(m_ActiveMode.name) - 1);
    m_ActiveMode.actuator_config = comp.actuator_config;
    m_ActiveMode.sensor_config   = comp.sensor_config;
    m_HasActiveMode = true;
    WL_INFO_TAG("ROBOT_STATUS", "Selected item set to: {}", comp.name);
}

void RobotStatus::SetActiveGamepad(GamepadMapper* gp)
{
    std::unique_lock<std::shared_mutex> lock(m_StatusMutex);
    m_ActiveGamepad = gp;
    if (gp)
        WL_INFO_TAG("ROBOT_STATUS", "Active gamepad set to: {}", gp->name);
    else
        WL_INFO_TAG("ROBOT_STATUS", "Active gamepad cleared");
}

void RobotStatus::LoadGraph(const std::string& gpMapperName)
{
    std::unique_lock<std::shared_mutex> lock(m_StatusMutex);
    if (!m_GraphEvaluator || !m_HasActiveMode) {
        if (m_GraphEvaluator) m_GraphEvaluator->Clear();
        return;
    }

    // 优先从 node_graph_pairs 查找
    auto it = m_ActiveMode.node_graph_pairs.find(gpMapperName);
    if (it != m_ActiveMode.node_graph_pairs.end()) {
        if (m_GraphEvaluator->LoadGraphData(it->second))
            WL_INFO_TAG("ROBOT_STATUS", "Loaded graph for item '{}' + gamepad '{}'",
                        m_ActiveMode.name, gpMapperName);
        else
            WL_WARN_TAG("ROBOT_STATUS", "Failed to parse graph for item '{}' + gamepad '{}'",
                        m_ActiveMode.name, gpMapperName);
    } else if (!m_ActiveMode.node_graph.empty()) {
        m_GraphEvaluator->LoadGraphData(m_ActiveMode.node_graph);
        WL_INFO_TAG("ROBOT_STATUS", "Loaded legacy graph for item '{}'", m_ActiveMode.name);
    } else {
        m_GraphEvaluator->Clear();
        WL_INFO_TAG("ROBOT_STATUS", "No graph for item '{}' + gamepad '{}', cleared",
                    m_ActiveMode.name, gpMapperName);
    }
}

// ---- 节点图求值（读锁） ----

void RobotStatus::EvaluateIntoActuator(const std::map<std::string, float>& keyValues, ActuatorConfig& data)
{
    std::shared_lock lock(m_StatusMutex);
    if (m_GraphEvaluator)
        m_GraphEvaluator->EvaluateIntoActuator(keyValues, data);
}

void RobotStatus::EvaluateIntoActuators(const std::map<std::string, float>& keyValues,
                                         std::vector<ActuatorConfig>& dataVec,
                                         std::set<int>* pWrittenIndices)
{
    std::shared_lock lock(m_StatusMutex);
    if (m_GraphEvaluator)
        m_GraphEvaluator->EvaluateIntoActuators(keyValues, dataVec, pWrittenIndices);
}

// ---- 外联 getter（读锁） ----

bool RobotStatus::HasGraphEvaluator()    const { std::shared_lock lock(m_StatusMutex); return m_GraphEvaluator != nullptr; }
bool RobotStatus::HasActiveMode()        const { std::shared_lock lock(m_StatusMutex); return m_HasActiveMode; }
const RobotMode* RobotStatus::GetActiveModePtr() const { std::shared_lock lock(m_StatusMutex); return m_HasActiveMode ? &m_ActiveMode : nullptr; }
GamepadMapper* RobotStatus::GetActiveGamepadPtr() const { std::shared_lock lock(m_StatusMutex); return m_ActiveGamepad; }
std::string RobotStatus::GetActiveModeName() const { std::shared_lock lock(m_StatusMutex); return m_HasActiveMode ? std::string(m_ActiveMode.name) : ""; }

const ActuatorConfig&        RobotStatus::GetAppliedActuator()   const { std::shared_lock lock(m_StatusMutex); return m_ActiveMode.actuator_config; }
const std::vector<ProtocolSendConfig>& RobotStatus::GetAppliedSendConfig() const { std::shared_lock lock(m_StatusMutex); return m_ActiveMode.protocol_send; }
const std::vector<ProtocolReceiveConfig>& RobotStatus::GetAppliedRecvConfig() const { std::shared_lock lock(m_StatusMutex); return m_ActiveMode.protocol_receive; }

// ====== 快捷键：切换发送帧 ======

void RobotStatus::ToggleSendFrame(int index)
{
    if (!m_RobotCommManager) return;
    auto& nodes = m_RobotCommManager->GetNodes();

    // 每个 active comm 的 protocol_send 指针和时间归属映射
    struct Slot { int connIdx; ProtocolSendConfig* cfg; };
    std::vector<Slot> slots;
    for (int i = 0; i < (int)m_Connections.size(); ++i) {
        int ci = (i < (int)m_ActiveCommIndices.size()) ? m_ActiveCommIndices[i] : -1;
        if (ci < 0 || ci >= (int)nodes.size()) continue;
        for (auto& sc : nodes[ci].component.protocol_send)
            slots.push_back({i, &sc});
    }

    if (index < 0 || index >= (int)slots.size()) return;
    bool was = slots[index].cfg->enabled;
    slots[index].cfg->enabled = !was;

    // 只推对应连接的协议配置
    for (int i = 0; i < (int)m_Connections.size(); ++i) {
        auto& conn = m_Connections[i];
        if (!conn.isLinked) continue;
        int ci = (i < (int)m_ActiveCommIndices.size()) ? m_ActiveCommIndices[i] : -1;
        if (ci < 0 || ci >= (int)nodes.size()) continue;
        conn.hw->SetProtocolConfig(nodes[ci].component.protocol_send);
    }
}

void RobotStatus::OneShotSendFrame(int index)
{
    if (!m_RobotCommManager) return;
    auto& nodes = m_RobotCommManager->GetNodes();

    struct Slot { int connIdx; ProtocolSendConfig* cfg; };
    std::vector<Slot> slots;
    for (int i = 0; i < (int)m_Connections.size(); ++i) {
        int ci = (i < (int)m_ActiveCommIndices.size()) ? m_ActiveCommIndices[i] : -1;
        if (ci < 0 || ci >= (int)nodes.size()) continue;
        for (auto& sc : nodes[ci].component.protocol_send)
            slots.push_back({i, &sc});
    }

    if (index < 0 || index >= (int)slots.size()) return;
    int owner = slots[index].connIdx;

    // 临时启用
    bool wasEnabled = slots[index].cfg->enabled;
    slots[index].cfg->enabled = true;

    // 只推对应连接的协议配置
    for (int i = 0; i < (int)m_Connections.size(); ++i) {
        auto& conn = m_Connections[i];
        if (!conn.isLinked) continue;
        int ci = (i < (int)m_ActiveCommIndices.size()) ? m_ActiveCommIndices[i] : -1;
        if (ci < 0 || ci >= (int)nodes.size()) continue;
        conn.hw->SetProtocolConfig(nodes[ci].component.protocol_send);
    }

    // 发一次
    if (owner >= 0 && owner < (int)m_Connections.size() && m_Connections[owner].isLinked) {
        auto cmdPtr = GetCurrentCommand();
        if (cmdPtr) m_Connections[owner].hw->SendActuatorData(*cmdPtr);
    }

    // 恢复
    slots[index].cfg->enabled = wasEnabled;
    for (int i = 0; i < (int)m_Connections.size(); ++i) {
        auto& conn = m_Connections[i];
        if (!conn.isLinked) continue;
        int ci = (i < (int)m_ActiveCommIndices.size()) ? m_ActiveCommIndices[i] : -1;
        if (ci < 0 || ci >= (int)nodes.size()) continue;
        conn.hw->SetProtocolConfig(nodes[ci].component.protocol_send);
    }
}

void RobotStatus::ToggleAllSendFrames()
{
    if (!m_RobotCommManager) return;
    auto& nodes = m_RobotCommManager->GetNodes();

    // 收集所有 active comm 的 protocol_send
    struct Slot { int connIdx; ProtocolSendConfig* cfg; };
    std::vector<Slot> slots;
    for (int i = 0; i < (int)m_Connections.size(); ++i) {
        int ci = (i < (int)m_ActiveCommIndices.size()) ? m_ActiveCommIndices[i] : -1;
        if (ci < 0 || ci >= (int)nodes.size()) continue;
        for (auto& sc : nodes[ci].component.protocol_send)
            slots.push_back({i, &sc});
    }

    // 全开/全关
    bool allOn = !slots.empty();
    for (auto& s : slots)
        if (!s.cfg->enabled) { allOn = false; break; }
    for (auto& s : slots)
        s.cfg->enabled = !allOn;

    // 只推各自连接的协议配置
    for (int i = 0; i < (int)m_Connections.size(); ++i) {
        auto& conn = m_Connections[i];
        if (!conn.isLinked) continue;
        int ci = (i < (int)m_ActiveCommIndices.size()) ? m_ActiveCommIndices[i] : -1;
        if (ci < 0 || ci >= (int)nodes.size()) continue;
        conn.hw->SetProtocolConfig(nodes[ci].component.protocol_send);
    }
}
const SensorConfig&          RobotStatus::GetSensorConfig()      const { std::shared_lock lock(m_StatusMutex); return m_ActiveMode.sensor_config; }
bool RobotStatus::HasTemperature() const { std::shared_lock lock(m_StatusMutex); return m_HasActiveMode && m_ActiveMode.sensor_config.has_temperature; }
bool RobotStatus::HasHumidity()    const { std::shared_lock lock(m_StatusMutex); return m_HasActiveMode && m_ActiveMode.sensor_config.has_humidity; }
bool RobotStatus::HasDepth()       const { std::shared_lock lock(m_StatusMutex); return m_HasActiveMode && m_ActiveMode.sensor_config.has_depth; }

// ---- 运行时数据更新（读锁保护 m_CurrentSensor/m_SensorValid） ----

void RobotStatus::UpdateCommandData(std::shared_ptr<const ActuatorConfig> cmd)
{
    if (cmd) {
        std::unique_lock lock(m_StatusMutex);
        m_CurrentCommand = std::move(cmd);
    }
}

void RobotStatus::UpdateSensorData(const SensorData& sensor, bool valid)
{
    std::unique_lock lock(m_StatusMutex);
    m_CurrentSensor = sensor;
    m_SensorValid   = valid;
}

// ---- 运行时数据访问（读锁） ----

std::shared_ptr<const ActuatorConfig> RobotStatus::GetCurrentCommand() const
{
    std::shared_lock lock(m_StatusMutex);
    return m_CurrentCommand;
}

SensorData RobotStatus::GetCurrentSensor() const
{
    std::shared_lock lock(m_StatusMutex);
    return m_CurrentSensor;
}

bool RobotStatus::IsSensorValid() const
{
    std::shared_lock lock(m_StatusMutex);
    return m_SensorValid;
}

// ---- 多连接控制（由 NodeGraph 的 CommRefs 决定，自动同步）----

void RobotStatus::SyncConnectionsFromGraph()
{
    if (!m_GraphEvaluator || !m_RobotCommManager) return;

    const auto& commRefs = m_GraphEvaluator->GetCommRefs();
    int commMgrCount = m_RobotCommManager->GetItemCount();
    auto allCfgs = m_RobotCommManager->GetAllItems();

    // 只同步连接池的 config 部分，绝不改变 isLinked 状态
    bool changed = ((int)m_Connections.size() != (int)commRefs.size());
    if (!changed) {
        for (int i = 0; i < (int)commRefs.size(); ++i) {
            int ci = commRefs[i];
            if (i >= (int)m_Connections.size()) { changed = true; break; }
            const char* wantedName = (ci >= 0 && ci < (int)allCfgs.size()) ? allCfgs[ci].name : "";
            if (strcmp(m_Connections[i].config.name, wantedName) != 0) { changed = true; break; }
        }
    }

    if (changed) {
        // 保存旧连接状态
        std::unordered_map<std::string, bool> linkedMap;
        for (auto& conn : m_Connections)
            if (conn.isLinked) linkedMap[conn.config.name] = true;

        m_Connections.clear();
        for (int ci : commRefs) {
            if (ci < 0 || ci >= commMgrCount) continue;
            ConnectionEntry entry;
            entry.hw = std::make_shared<HardwareInterface>();
            entry.config = allCfgs[ci];
            // 仅当之前已连接时重建（用户手动点过 Connect），否则保持未连接
            entry.isLinked = (linkedMap.count(entry.config.name) > 0);
            if (entry.isLinked) {
                entry.hw->Initialize(entry.config.host_ip, entry.config.remote_port,
                                     entry.config.local_port, entry.config.transport_type);
                if (!entry.hw->IsConnected()) entry.isLinked = false;
            }
            m_Connections.push_back(std::move(entry));
        }
        WL_INFO_TAG("ROBOT_STATUS", "Synced {} connections from graph comm_refs (linked={})",
                    m_Connections.size(),
                    (int)std::count_if(m_Connections.begin(), m_Connections.end(),
                                       [](auto& c){ return c.isLinked; }));
    }
}

bool RobotStatus::IsLinked() const
{
    for (const auto& conn : m_Connections)
        if (conn.isLinked) return true;
    return false;
}

const ConnectionEntry* RobotStatus::GetConnection(int index) const
{
    if (index >= 0 && index < (int)m_Connections.size())
        return &m_Connections[index];
    return nullptr;
}

bool RobotStatus::LinkConnection(int index)
{
    if (index < 0 || index >= (int)m_Connections.size()) return false;
    auto& conn = m_Connections[index];
    WL_INFO_TAG("ROBOT_STATUS", "Linking {} ({}:{})...", conn.config.name, conn.config.host_ip, conn.config.remote_port);
    bool ok = conn.hw->Initialize(conn.config.host_ip, conn.config.remote_port,
                                   conn.config.local_port, conn.config.transport_type);
    if (ok) {
        // Push protocol configs from RobotComm to hardware interface
        conn.hw->SetProtocolConfig(conn.config.protocol_send);
        conn.hw->SetProtocolReceiveConfig(conn.config.protocol_receive);
    }
    conn.isLinked = ok;
    return ok;
}

void RobotStatus::UnlinkConnection(int index)
{
    if (index < 0 || index >= (int)m_Connections.size()) return;
    auto& conn = m_Connections[index];
    if (conn.isLinked)
        WL_INFO_TAG("ROBOT_STATUS", "Unlinked {} ({})", conn.config.name, conn.config.host_ip);
    conn.isLinked = false;
}

void RobotStatus::UnlinkAll()
{
    for (auto& conn : m_Connections) conn.isLinked = false;
    m_LastSyncedProtocolKey.clear();
}

void RobotStatus::SendActuatorData(const ActuatorConfig& data)
{
    for (auto& conn : m_Connections) {
        if (!conn.isLinked) continue;
        conn.hw->SendActuatorData(data);
        if (!conn.hw->IsConnected()) {
            WL_ERROR_TAG("ROBOT_STATUS", "Connection lost during send: {} ({})",
                         conn.config.name, conn.config.host_ip);
            conn.isLinked = false;
        }
    }
}

SensorData RobotStatus::GetSensorData()
{
    for (auto& conn : m_Connections) {
        if (!conn.isLinked) continue;
        SensorData d = conn.hw->GetSensorData();
        if (!conn.hw->IsConnected()) {
            WL_ERROR_TAG("ROBOT_STATUS", "Connection lost during recv: {} ({})",
                         conn.config.name, conn.config.host_ip);
            conn.isLinked = false;
        }
        return d;
    }
    SensorData d; d.is_valid = false; return d;
}

void RobotStatus::DrawWindow(bool* p_open, RobotCommManager* commManager,
                              LiveStreamManager* liveStreamMgr,
                              NodeGraphManager* nodeGraphMgr,
                              RobotComponentManager* compMgr,
                              GamepadMapperManager* gpMgr)
{
    // 缓存 Manager 指针（每帧刷新，供 SyncActiveNodeGraph / DeriveActiveFromNodeGraph 等使用）
    m_NodeGraphManager        = nodeGraphMgr;
    m_RobotCommManager        = commManager;
    m_RobotComponentManager   = compMgr;
    m_GamepadMapperManager    = gpMgr;

    // 首帧自动同步 + 推导 Active（无需用户手动切换 NodeGraph combo）
    if (!m_DidFirstDerive && nodeGraphMgr && nodeGraphMgr->GetItemCount() > 0) {
        m_DidFirstDerive = true;
        SyncActiveNodeGraph();
        DeriveActiveFromNodeGraph();
    }

    // 响应外部请求（如 RestoreRobotStatusActive 后）同步 NodeGraph
    if (m_NeedsNodeGraphSync) {
        m_NeedsNodeGraphSync = false;
        SyncActiveNodeGraph();
    }

    if (!ImGui::Begin("Robot Status", p_open))
    {
        ImGui::End();
        return;
    }

    // ---- 顶部 Active 选择器（RobotStatus 自己的选择，不碰 Manager 的 Select） ----
    if (liveStreamMgr && liveStreamMgr->GetItemCount() > 0)
    {
        if (m_ActiveLiveStreamIdx >= liveStreamMgr->GetItemCount())
            m_ActiveLiveStreamIdx = 0;
        ImGui::TextUnformatted("Live Stream:");
        ImGui::SetNextItemWidth(-1);
        const char* preview = liveStreamMgr->GetItemNameBuf(m_ActiveLiveStreamIdx);
        if (ImGui::BeginCombo("##LSActive", preview))
        {
            for (int i = 0; i < liveStreamMgr->GetItemCount(); ++i)
            {
                bool sel = (i == m_ActiveLiveStreamIdx);
                if (ImGui::Selectable(liveStreamMgr->GetItemNameBuf(i), sel))
                    m_ActiveLiveStreamIdx = i;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    if (nodeGraphMgr && nodeGraphMgr->GetItemCount() > 0)
    {
        if (m_ActiveNodeGraphIdx >= nodeGraphMgr->GetItemCount())
            m_ActiveNodeGraphIdx = 0;
        ImGui::TextUnformatted("Node Graph:");
        ImGui::SetNextItemWidth(-1);
        const char* preview = nodeGraphMgr->GetItemNameBuf(m_ActiveNodeGraphIdx);
        if (ImGui::BeginCombo("##NGActive", preview))
        {
            for (int i = 0; i < nodeGraphMgr->GetItemCount(); ++i)
            {
                bool sel = (i == m_ActiveNodeGraphIdx);
                if (ImGui::Selectable(nodeGraphMgr->GetItemNameBuf(i), sel)) {
                    if (i != m_ActiveNodeGraphIdx) {
                        m_ActiveNodeGraphIdx = i;
                        SyncActiveNodeGraph();         // 同步图数据到求值器
                        DeriveActiveFromNodeGraph();   // 从 NodeGraph 推导 ActiveMode / ActiveGamepad / Comm
                    }
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    // ---- 连接状态（由 NodeGraph CommRefs 自动驱动，不可手动增减） ----
    SyncConnectionsFromGraph();

    if (commManager && commManager->GetItemCount() > 0)
    {
        ImGui::Spacing();
        int linkedCount = 0;
        for (const auto& conn : m_Connections)
            if (conn.isLinked) ++linkedCount;
        bool anyLinked = (linkedCount > 0);

        ImVec4 statusColor = anyLinked ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
        ImGui::TextColored(statusColor, "Status: %d/%d linked", linkedCount, (int)m_Connections.size());

        if (m_Connections.empty()) {
            ImGui::TextDisabled("  (NodeGraph has no comm refs — add in sidebar)");
        } else if (ImGui::TreeNodeEx("Connections", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            for (int i = 0; i < (int)m_Connections.size(); ++i) {
                auto& conn = m_Connections[i];
                ImGui::PushID(i);
                ImVec4 col = conn.isLinked ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                ImGui::TextColored(col, "[%d] %s", i, conn.config.name);
                ImGui::SameLine();
                ImGui::TextDisabled("(%s:%d)", conn.config.host_ip, conn.config.remote_port);
                if (conn.isLinked) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Disconnect")) UnlinkConnection(i);
                } else {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Connect")) LinkConnection(i);
                }
                ImGui::PopID();
            }
            ImGui::Unindent();
            ImGui::TreePop();
        }

        // 一键操作
        if (anyLinked) {
            if (ImGui::Button("Disconnect All", ImVec2(-1, 0))) UnlinkAll();
        } else if (!m_Connections.empty()) {
            if (ImGui::Button("Connect All", ImVec2(-1, 0)))
                for (int i = 0; i < (int)m_Connections.size(); ++i) LinkConnection(i);
        }
    }

    // ---- 当前 Active 项一览 ----
    {
        ImGui::Spacing();
        ImGui::TextDisabled("Active Items:");

        RobotMode mode;
        GamepadMapper* gp = nullptr;
        {
            std::shared_lock lock(m_StatusMutex);
            if (m_HasActiveMode) mode = m_ActiveMode;
            gp = m_ActiveGamepad;
        }

        ImGui::Text("  Component:    %s", m_HasActiveMode ? mode.name : "(none)");
        ImGui::Text("  Gamepad:      %s", gp ? gp->name : "(none)");

        if (nodeGraphMgr && m_ActiveNodeGraphIdx < nodeGraphMgr->GetItemCount())
            ImGui::Text("  NodeGraph:    %s", nodeGraphMgr->GetItemNameBuf(m_ActiveNodeGraphIdx));
        else
            ImGui::Text("  NodeGraph:    (none)");

        if (liveStreamMgr && m_ActiveLiveStreamIdx < liveStreamMgr->GetItemCount())
            ImGui::Text("  LiveStream:   %s", liveStreamMgr->GetItemNameBuf(m_ActiveLiveStreamIdx));
        else
            ImGui::Text("  LiveStream:   (none)");

        if (commManager && !m_ActiveCommIndices.empty()) {
            ImGui::Text("  Comm Configs:");
            for (int ci : m_ActiveCommIndices) {
                if (ci < commManager->GetItemCount())
                    ImGui::Text("    - %s", commManager->GetItemNameBuf(ci));
            }
        } else {
            ImGui::Text("  Comm Config:  (none)");
        }
    }

    ImGui::Separator();

    // 快照：在共享锁下获取所有需要的数据，然后用局部变量绘制
    RobotMode snapshotMode;
    std::shared_ptr<const ActuatorConfig> cmd;
    bool linked;
    {
        std::shared_lock lock(m_StatusMutex);
        snapshotMode = m_ActiveMode;
        cmd  = m_CurrentCommand;
        linked = IsLinked();
    }
    if (!m_HasActiveMode) { ImGui::TextDisabled("No selected item"); ImGui::End(); return; }

    // 未连接时，使用 RobotComponent 中配置的初始值；连接后使用实时求值结果
    if (!cmd || !linked)
        cmd = std::make_shared<const ActuatorConfig>(snapshotMode.actuator_config);

    // 从活跃 Comm 配置中收集协议字段引用（直接操作原始数据，无需写回）
    std::vector<std::pair<int, ProtocolSendConfig*>> activeSendCfgs;   // (commIdx, ptr)
    std::vector<std::pair<int, ProtocolReceiveConfig*>> activeRecvCfgs;
    if (commManager) {
        auto& nodes = commManager->GetNodes();
        for (int ci : m_ActiveCommIndices) {
            if (ci < 0 || ci >= (int)nodes.size()) continue;
            auto& cfg = nodes[ci].component;
            for (auto& sc : cfg.protocol_send)
                activeSendCfgs.push_back({ci, &sc});
            for (auto& rc : cfg.protocol_receive)
                activeRecvCfgs.push_back({ci, &rc});
        }
    }

    // === Send Control ===
    if (ImGui::CollapsingHeader("Send Control", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (activeSendCfgs.empty())
        {
            ImGui::TextDisabled("  No send frames configured");
        }
        else
        {
            ImGui::Indent();

            // 每次连接后首次同步协议配置到硬件
            if (IsLinked() && m_LastSyncedProtocolKey != snapshotMode.name) {
                std::vector<ProtocolSendConfig> sendCopy;
                std::vector<ProtocolReceiveConfig> recvCopy;
                for (auto& p : activeSendCfgs)  sendCopy.push_back(*p.second);
                for (auto& p : activeRecvCfgs)  recvCopy.push_back(*p.second);
                for (auto& conn : m_Connections) {
                    if (!conn.isLinked) continue;
                    conn.hw->SetProtocolConfig(sendCopy);
                    conn.hw->SetProtocolReceiveConfig(recvCopy);
                }
                m_LastSyncedProtocolKey = snapshotMode.name;
            }

            bool anyChanged = false;
            for (size_t i = 0; i < activeSendCfgs.size(); ++i)
            {
                auto& sc = *activeSendCfgs[i].second;
                ImGui::PushID((int)i);
                if (ImGui::Checkbox("##sendEn", &sc.enabled))
                    anyChanged = true;
                ImGui::PopID();
                ImGui::SameLine();
                ImGui::TextColored(sc.enabled ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                    "%s  [cmd:%zub] %s (%zu fields)",
                    sc.name, sc.command_bytes.size(), sc.enabled ? "ON" : "OFF", sc.fields.size());
            }
            if (anyChanged && IsLinked()) {
                std::vector<ProtocolSendConfig> sendCopy;
                for (auto& p : activeSendCfgs) sendCopy.push_back(*p.second);
                for (auto& conn : m_Connections) {
                    if (!conn.isLinked) continue;
                    conn.hw->SetProtocolConfig(sendCopy);
                }
            }
            ImGui::Unindent();
        }
    }

    // === Actuator ===
    if (ImGui::CollapsingHeader("Actuator", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (activeSendCfgs.empty())
        {
            ImGui::TextDisabled("  No send frames configured");
        }
        else
        {
            std::map<std::string, std::vector<const SendField*>> groups;
            for (const auto& sc : activeSendCfgs)
                for (const auto& f : sc.second->fields)
                    groups[f.group.empty() ? "Default" : f.group].push_back(&f);

            ImGui::Indent();
            for (const auto& g : groups)
            {
                if (ImGui::TreeNodeEx(g.first.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    int visibleCnt = 0;
                    for (const auto* f : g.second)
                    {
                        if (!f->visible) continue;
                        ++visibleCnt;

                        double val = 0.0;
                        GetActuatorField(*cmd, f->field_path, val);
                        ImGui::Text("  %s = %.2f", f->name.c_str(), val);
                    }
                    if (visibleCnt == 0)
                        ImGui::TextDisabled("  (no visible fields)");
                    ImGui::TreePop();
                }
            }
            ImGui::Unindent();
        }
    }

    // === Sensor ===
    if (ImGui::CollapsingHeader("Sensor", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (activeRecvCfgs.empty())
        {
            ImGui::TextDisabled("  No receive frames configured");
        }
        else
        {
            std::map<std::string, std::vector<const ReceiveField*>> groups;
            for (const auto& rc : activeRecvCfgs)
                for (const auto& f : rc.second->fields)
                    groups[f.group.empty() ? "Default" : f.group].push_back(&f);

            ImGui::Indent();
            for (const auto& g : groups)
            {
                if (ImGui::TreeNodeEx(g.first.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    int visibleCnt = 0;
                    SensorData sensorData;
                    bool hasData = false;
                    if (IsLinked())
                    {
                        sensorData = GetSensorData();
                        hasData = sensorData.is_valid;
                    }

                    for (const auto* f : g.second)
                    {
                        if (!f->visible) continue;
                        ++visibleCnt;

                        double val = 0.0;
                        bool hasVal = hasData && GetSensorField(sensorData, f->field_path, val);

                        if (hasVal)
                            ImGui::Text("  %s = %.2f", f->name.c_str(), val);
                        else
                            ImGui::Text("  %s = --", f->name.c_str());
                    }
                    if (visibleCnt == 0)
                        ImGui::TextDisabled("  (no visible fields)");
                    ImGui::TreePop();
                }
            }
            ImGui::Unindent();
        }
    }

    // === Graph Variables — 显示节点图中标记为 visible 的中间变量 ===
    {
        std::shared_lock lock(m_StatusMutex);
        if (m_GraphEvaluator)
        {
            const auto& globals = m_GraphEvaluator->GetGlobals();
            int visibleCount = 0;
            for (const auto& gv : globals)
                if (gv.visible) { ++visibleCount; }

            if (ImGui::CollapsingHeader("Graph Variables", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (visibleCount == 0)
                {
                    ImGui::TextDisabled("  (no visible variables)");
                }
                else
                {
                    ImGui::Indent();
                    for (const auto& gv : globals)
                    {
                        if (!gv.visible) continue;

                        switch (gv.type) {
                        case PinType::Bool:
                            if (gv.value >= 0.5f)
                                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "  %s = True", gv.name.c_str());
                            else
                                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "  %s = False", gv.name.c_str());
                            break;
                        case PinType::Int:
                            ImGui::Text("  %s = %d", gv.name.c_str(), (int)gv.value);
                            break;
                        default:
                            ImGui::Text("  %s = %.2f", gv.name.c_str(), gv.value);
                            break;
                        }
                    }
                    ImGui::Unindent();
                }
            }
        }
    }

    ImGui::End();
}

