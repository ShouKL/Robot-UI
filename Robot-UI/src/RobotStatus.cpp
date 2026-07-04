#include "RobotStatus.h"

RobotStatus::RobotStatus()
{
    m_CurrentCommands.resize(1);
    m_CurrentCommands[0] = std::make_shared<const ActuatorConfig>();
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

    // 仅加载图数据到求值器，不改写 Status 自己的 ActiveNodeGraphIdx/ActiveCommIndices/ActiveMode/ActiveGamepad
    // Setting 面板的 select 是独立运行态，关闭面板后由 RestoreRobotStatusActive 恢复 Status 的 active
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
    std::shared_lock<std::shared_mutex> lock(m_ConnMutex);
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
    strncpy_s(m_ActiveMode.name, comp.name, sizeof(m_ActiveMode.name) - 1);
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

// ======    /    ======

void RobotStatus::ToggleSendFrame(int index)
{
    if (!m_RobotCommManager) return;
    auto& nodes = m_RobotCommManager->GetNodes();

    std::unique_lock<std::shared_mutex> connLock(m_ConnMutex);

    //   active comm   protocol_send      /     
    struct Slot { int connIdx; ProtocolSendConfig* cfg; };
    std::vector<Slot> slots;
    for (int i = 0; i < (int)m_Connections.size(); ++i) {
        int ci = (i < (int)m_ActiveCommIndices.size()) ? m_ActiveCommIndices[i] : -1;
        if (ci < 0 || ci >= (int)nodes.size()) continue;
        for (auto& sc : nodes[ci]->protocol_send)
            slots.push_back({i, &sc});
    }

    if (index < 0 || index >= (int)slots.size()) return;
    bool was = slots[index].cfg->enabled;
    slots[index].cfg->enabled = !was;

    //          
    for (int i = 0; i < (int)m_Connections.size(); ++i) {
        auto& conn = m_Connections[i];
        if (!conn.isLinked) continue;
        int ci = (i < (int)m_ActiveCommIndices.size()) ? m_ActiveCommIndices[i] : -1;
        if (ci < 0 || ci >= (int)nodes.size()) continue;
        conn.hw->SetProtocolConfig(nodes[ci]->protocol_send);
    }
}

void RobotStatus::OneShotSendFrame(int index)
{
    if (!m_RobotCommManager) return;
    auto& nodes = m_RobotCommManager->GetNodes();

    std::unique_lock<std::shared_mutex> connLock(m_ConnMutex);

    struct Slot { int connIdx; ProtocolSendConfig* cfg; };
    std::vector<Slot> slots;
    for (int i = 0; i < (int)m_Connections.size(); ++i) {
        int ci = (i < (int)m_ActiveCommIndices.size()) ? m_ActiveCommIndices[i] : -1;
        if (ci < 0 || ci >= (int)nodes.size()) continue;
        for (auto& sc : nodes[ci]->protocol_send)
            slots.push_back({i, &sc});
    }

    if (index < 0 || index >= (int)slots.size()) return;
    int owner = slots[index].connIdx;

    //    
    bool wasEnabled = slots[index].cfg->enabled;
    slots[index].cfg->enabled = true;

    //          
    for (int i = 0; i < (int)m_Connections.size(); ++i) {
        auto& conn = m_Connections[i];
        if (!conn.isLinked) continue;
        int ci = (i < (int)m_ActiveCommIndices.size()) ? m_ActiveCommIndices[i] : -1;
        if (ci < 0 || ci >= (int)nodes.size()) continue;
        conn.hw->SetProtocolConfig(nodes[ci]->protocol_send);
    }

    //    
    if (owner >= 0 && owner < (int)m_Connections.size() && m_Connections[owner].isLinked) {
        auto cmdPtr = GetCurrentCommand(owner);
        if (cmdPtr) m_Connections[owner].hw->SendActuatorData(*cmdPtr);
    }

    //  
    slots[index].cfg->enabled = wasEnabled;
    for (int i = 0; i < (int)m_Connections.size(); ++i) {
        auto& conn = m_Connections[i];
        if (!conn.isLinked) continue;
        int ci = (i < (int)m_ActiveCommIndices.size()) ? m_ActiveCommIndices[i] : -1;
        if (ci < 0 || ci >= (int)nodes.size()) continue;
        conn.hw->SetProtocolConfig(nodes[ci]->protocol_send);
    }
}

void RobotStatus::ToggleAllSendFrames()
{
    if (!m_RobotCommManager) return;
    auto& nodes = m_RobotCommManager->GetNodes();

    std::unique_lock<std::shared_mutex> connLock(m_ConnMutex);

    //    active comm   protocol_send
    struct Slot { int connIdx; ProtocolSendConfig* cfg; };
    std::vector<Slot> slots;
    for (int i = 0; i < (int)m_Connections.size(); ++i) {
        int ci = (i < (int)m_ActiveCommIndices.size()) ? m_ActiveCommIndices[i] : -1;
        if (ci < 0 || ci >= (int)nodes.size()) continue;
        for (auto& sc : nodes[ci]->protocol_send)
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
        conn.hw->SetProtocolConfig(nodes[ci]->protocol_send);
    }
}
const SensorConfig&          RobotStatus::GetSensorConfig()      const { std::shared_lock lock(m_StatusMutex); return m_ActiveMode.sensor_config; }
bool RobotStatus::HasTemperature() const { std::shared_lock lock(m_StatusMutex); return m_HasActiveMode && m_ActiveMode.sensor_config.has_temperature; }
bool RobotStatus::HasHumidity()    const { std::shared_lock lock(m_StatusMutex); return m_HasActiveMode && m_ActiveMode.sensor_config.has_humidity; }
bool RobotStatus::HasDepth()       const { std::shared_lock lock(m_StatusMutex); return m_HasActiveMode && m_ActiveMode.sensor_config.has_depth; }

// ---- 运行时数据更新（读锁保护 m_CurrentSensor/m_SensorValid） ----

void RobotStatus::UpdateCommandData(int index, std::shared_ptr<const ActuatorConfig> cmd)
{
    if (cmd) {
        std::unique_lock lock(m_StatusMutex);
        if (index >= (int)m_CurrentCommands.size())
            m_CurrentCommands.resize(index + 1);
        m_CurrentCommands[index] = std::move(cmd);
    }
}

void RobotStatus::UpdateAllCommandData(const std::vector<std::shared_ptr<const ActuatorConfig>>& cmds)
{
    std::unique_lock lock(m_StatusMutex);
    m_CurrentCommands = cmds;
}

void RobotStatus::UpdateSensorData(const SensorData& sensor, bool valid)
{
    std::unique_lock lock(m_StatusMutex);
    m_CurrentSensor = sensor;
    m_SensorValid   = valid;
}

// ---- 运行时数据访问（读锁） ----

std::shared_ptr<const ActuatorConfig> RobotStatus::GetCurrentCommand(int index) const
{
    std::shared_lock lock(m_StatusMutex);
    if (index >= 0 && index < (int)m_CurrentCommands.size())
        return m_CurrentCommands[index];
    return nullptr;
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
    if (!m_RobotCommManager) return;

    // 使用 Status 自己的 m_ActiveCommIndices（来自 Status 的 active NodeGraph），
    // 而非 m_GraphEvaluator->GetCommRefs()（后者在 live sync 期间被 Manager 选中图覆盖）
    const auto& commRefs = m_ActiveCommIndices;
    int commMgrCount = m_RobotCommManager->GetItemCount();
    auto allCfgs = m_RobotCommManager->GetAllItems();

    std::unique_lock<std::shared_mutex> connLock(m_ConnMutex);

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
        for (auto& conn : m_Connections) {
            if (conn.isLinked) {
                linkedMap[conn.config.name] = true;
                conn.hw->Shutdown();  // 关闭旧连接 socket
            }
        }

        m_Connections.clear();
        for (int ci : commRefs) {
            if (ci < 0 || ci >= commMgrCount) continue;
            ConnectionEntry entry;
            entry.hw = std::make_shared<HardwareInterface>();
            entry.config = allCfgs[ci];
            // 仅当之前已连接时重建（用户手动点过 Connect），否则保持未连接
            entry.isLinked = (linkedMap.count(entry.config.name) > 0);
            if (entry.isLinked) {
                if (entry.config.transport_type == 2)
                    entry.hw->InitSerial(entry.config.com_port_str, entry.config.baud_rate,
                                         entry.config.data_bits, entry.config.stop_bits, entry.config.parity);
                else
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
    std::shared_lock<std::shared_mutex> lock(m_ConnMutex);
    for (const auto& conn : m_Connections)
        if (conn.isLinked) return true;
    return false;
}

const ConnectionEntry* RobotStatus::GetConnection(int index) const
{
    std::shared_lock<std::shared_mutex> lock(m_ConnMutex);
    if (index >= 0 && index < (int)m_Connections.size())
        return &m_Connections[index];
    return nullptr;
}

int RobotStatus::GetConnectionCount() const
{
    std::shared_lock<std::shared_mutex> lock(m_ConnMutex);
    return (int)m_Connections.size();
}

// ---- 线程安全快照（供外部线程使用） ----
std::vector<ConnectionSnapshot> RobotStatus::SnapshotConnections() const
{
    std::vector<ConnectionSnapshot> snap;
    std::shared_lock<std::shared_mutex> lock(m_ConnMutex);
    snap.reserve(m_Connections.size());
    for (int i = 0; i < (int)m_Connections.size(); ++i) {
        const auto& conn = m_Connections[i];
        ConnectionSnapshot s;
        s.hw        = conn.hw;
        s.config    = conn.config;
        s.isLinked  = conn.isLinked;
        s.commIndex = (i < (int)m_ActiveCommIndices.size()) ? m_ActiveCommIndices[i] : 0;
        snap.push_back(std::move(s));
    }
    return snap;
}

bool RobotStatus::LinkConnection(int index)
{
    std::unique_lock<std::shared_mutex> lock(m_ConnMutex);
    if (index < 0 || index >= (int)m_Connections.size()) return false;
    auto& conn = m_Connections[index];
    int totalAttempts = std::max(1, std::min(20, m_ConnRetryCount));
    WL_INFO_TAG("ROBOT_STATUS", "Linking {} ({}) [max {} attempts]...", conn.config.name,
        conn.config.transport_type == 2 ? conn.config.com_port_str : conn.config.host_ip,
        totalAttempts);

    // 首次尝试（同时将连接参数写入 hardware 内部成员变量）
    bool ok;
    if (conn.config.transport_type == 2) {
        ok = conn.hw->InitSerial(conn.config.com_port_str, conn.config.baud_rate,
                                  conn.config.data_bits, conn.config.stop_bits, conn.config.parity);
    } else {
        ok = conn.hw->Initialize(conn.config.host_ip, conn.config.remote_port,
                                  conn.config.local_port, conn.config.transport_type);
    }

    // 如果首次连接失败，启用重试机制
    if (!ok) {
        WL_WARN_TAG("ROBOT_STATUS", "Attempt 1 of {} failed for '{}', retrying...",
                     totalAttempts, conn.config.name);
        ok = conn.hw->HardwareInit(totalAttempts - 1, 2); // 从第 2 次开始，最多再试 5 次
    }

    if (ok) {
        WL_INFO_TAG("ROBOT_STATUS", "Link SUCCESS: {} ({})",
                     conn.config.name,
                     conn.config.transport_type == 2 ? conn.config.com_port_str : conn.config.host_ip);
        // Push protocol configs from RobotComm to hardware interface
        conn.hw->SetProtocolConfig(conn.config.protocol_send);
        conn.hw->SetProtocolReceiveConfig(conn.config.protocol_receive);
    } else {
        WL_ERROR_TAG("ROBOT_STATUS", "Link FAILED after all retries: {} ({})",
                     conn.config.name,
                     conn.config.transport_type == 2 ? conn.config.com_port_str : conn.config.host_ip);
    }
    conn.isLinked = ok;
    return ok;
}

void RobotStatus::UnlinkConnection(int index)
{
    std::unique_lock<std::shared_mutex> lock(m_ConnMutex);
    if (index < 0 || index >= (int)m_Connections.size()) return;
    auto& conn = m_Connections[index];
    if (conn.isLinked) {
        WL_INFO_TAG("ROBOT_STATUS", "Unlinked {} ({})", conn.config.name,
            conn.config.transport_type == 2 ? conn.config.com_port_str : conn.config.host_ip);
        conn.hw->Shutdown();  // 安全关闭底层 socket
    }
    conn.isLinked = false;
}

void RobotStatus::UnlinkAll()
{
    std::unique_lock<std::shared_mutex> lock(m_ConnMutex);
    for (auto& conn : m_Connections) {
        if (conn.isLinked) {
            WL_INFO_TAG("ROBOT_STATUS", "Unlinking {} ({})", conn.config.name,
                conn.config.transport_type == 2 ? conn.config.com_port_str : conn.config.host_ip);
            conn.hw->Shutdown();  // 安全关闭底层 socket
        }
        conn.isLinked = false;
    }
    m_LastSyncedProtocolKey.clear();
}

void RobotStatus::SendActuatorData(const ActuatorConfig& data)
{
    std::shared_lock<std::shared_mutex> lock(m_ConnMutex);
    for (auto& conn : m_Connections) {
        if (!conn.isLinked) continue;
        conn.hw->SendActuatorData(data);
        if (!conn.hw->IsConnected()) {
            WL_ERROR_TAG("ROBOT_STATUS", "Connection lost during send: {} ({})",
                         conn.config.name,
                         conn.config.transport_type == 2 ? conn.config.com_port_str : conn.config.host_ip);
            conn.isLinked = false;
        }
    }
}

SensorData RobotStatus::GetSensorData()
{
    std::shared_lock<std::shared_mutex> lock(m_ConnMutex);
    for (auto& conn : m_Connections) {
        if (!conn.isLinked) continue;
        SensorData d = conn.hw->GetSensorData();
        if (!conn.hw->IsConnected()) {
            WL_ERROR_TAG("ROBOT_STATUS", "Connection lost during recv: {} ({})",
                         conn.config.name,
                         conn.config.transport_type == 2 ? conn.config.com_port_str : conn.config.host_ip);
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

        // LiveStream Connect / Disconnect
        auto* activeStream = liveStreamMgr->GetDeviceByIndex(m_ActiveLiveStreamIdx);
        if (activeStream) {
            bool isStreamLinked = activeStream->isStreaming;
            bool isConnecting   = activeStream->IsConnecting();

            ImVec4 lsColor;
            if (isStreamLinked)      lsColor = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
            else if (isConnecting)   lsColor = ImVec4(1.0f, 0.7f, 0.2f, 1.0f);
            else                     lsColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
            ImGui::TextColored(lsColor, "  (%s:%d)", activeStream->ip, activeStream->port);
            ImGui::SameLine();
            if (isStreamLinked) {
                if (ImGui::SmallButton("Disconnect Stream")) {
                    activeStream->Close();
                    activeStream->isStreaming = false;
                }
            } else if (isConnecting) {
                if (ImGui::SmallButton("Cancel")) {
                    activeStream->CancelConnect();
                }
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "Connecting...");
            } else {
                if (ImGui::SmallButton("Connect Stream")) {
                    activeStream->TryOpen(m_CameraRetryCount);
                }
            }
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

    // ---- 连接状态（由 Status 自己的 active NodeGraph 决定） ----
    SyncConnectionsFromGraph();

    if (commManager && commManager->GetItemCount() > 0)
    {
        // 线程安全：快照连接数据用于 UI 渲染，避免持有锁期间进行 ImGui 绘制
        std::vector<ConnectionSnapshot> connSnap = SnapshotConnections();
        int linkedCount = 0;
        for (const auto& s : connSnap)
            if (s.isLinked) ++linkedCount;
        bool anyLinked = (linkedCount > 0);

        ImGui::Spacing();
        ImVec4 statusColor = anyLinked ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
        ImGui::TextColored(statusColor, "Status: %d/%d linked", linkedCount, (int)connSnap.size());

        if (connSnap.empty()) {
            ImGui::TextDisabled("  (NodeGraph has no comm refs — add in sidebar)");
        } else if (ImGui::TreeNodeEx("Connections", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            for (int i = 0; i < (int)connSnap.size(); ++i) {
                const auto& s = connSnap[i];
                ImGui::PushID(i);
                ImVec4 col = s.isLinked ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                ImGui::TextColored(col, "[%d] %s", i, s.config.name);
                ImGui::SameLine();
                if (s.config.transport_type == 2)
                    ImGui::TextDisabled("(Serial:%s @%d)", s.config.com_port_str, s.config.baud_rate);
                else
                    ImGui::TextDisabled("(%s:%d)", s.config.host_ip, s.config.remote_port);
                if (s.isLinked) {
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
            if (ImGui::Button("Disconnect All", ImVec2(-1, 0))) {
                UnlinkAll();
                if (liveStreamMgr) {
                    auto* stream = liveStreamMgr->GetDeviceByIndex(m_ActiveLiveStreamIdx);
                    if (stream && stream->isStreaming) {
                        stream->Close();
                        stream->isStreaming = false;
                    }
                }
            }
        } else if (!connSnap.empty()) {
            if (ImGui::Button("Connect All", ImVec2(-1, 0))) {
                for (int i = 0; i < (int)connSnap.size(); ++i) LinkConnection(i);
                if (liveStreamMgr) {
                    auto* stream = liveStreamMgr->GetDeviceByIndex(m_ActiveLiveStreamIdx);
                    if (stream && !stream->isStreaming) {
                        stream->TryOpen(m_CameraRetryCount);  // 后台线程，不阻塞 UI
                    }
                }
            }
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
    std::vector<std::shared_ptr<const ActuatorConfig>> cmds;
    bool linked;
    {
        std::shared_lock lock(m_StatusMutex);
        snapshotMode = m_ActiveMode;
        cmds  = m_CurrentCommands;
        linked = IsLinked();
    }
    if (!m_HasActiveMode) { ImGui::TextDisabled("No selected item"); ImGui::End(); return; }

    // 构建 comm 绝对索引 → connection 位置映射
    std::map<int, int> commToConnIdx;  // absolute comm index → connection index
    for (int i = 0; i < (int)m_ActiveCommIndices.size(); ++i)
        commToConnIdx[m_ActiveCommIndices[i]] = i;

    // 无求值结果时，使用 RobotComponent 中配置的初始值
    if (cmds.empty())
        cmds.push_back(std::make_shared<const ActuatorConfig>(snapshotMode.actuator_config));

    // 从活跃 Comm 配置中收集协议字段引用（直接操作原始数据，无需写回）
    std::vector<std::pair<int, ProtocolSendConfig*>> activeSendCfgs;   // (commIdx, ptr)
    std::vector<std::pair<int, ProtocolReceiveConfig*>> activeRecvCfgs;
    if (commManager) {
        auto& nodes = commManager->GetNodes();
        for (int ci : m_ActiveCommIndices) {
            if (ci < 0 || ci >= (int)nodes.size()) continue;
            auto* cfg = nodes[ci].get();
            for (auto& sc : cfg->protocol_send)
                activeSendCfgs.push_back({ci, &sc});
            for (auto& rc : cfg->protocol_receive)
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
            // Group by comm first, then by field group
            for (int connIdx = 0; connIdx < (int)m_ActiveCommIndices.size(); ++connIdx)
            {
                int absCommIdx = m_ActiveCommIndices[connIdx];

                // Collect fields belonging to this comm
                std::map<std::string, std::vector<const SendField*>> groups;
                for (const auto& sc : activeSendCfgs) {
                    if (sc.first != absCommIdx) continue;
                    for (const auto& f : sc.second->fields)
                        groups[f.group.empty() ? "Default" : f.group].push_back(&f);
                }
                if (groups.empty()) continue;

                // Get the right actuator config for this comm
                auto cmdPtr = (connIdx < (int)cmds.size()) ? cmds[connIdx] : nullptr;
                if (!cmdPtr) cmdPtr = std::make_shared<const ActuatorConfig>(snapshotMode.actuator_config);
                const auto& cmd = *cmdPtr;

                // Comm header
                const char* commName = "Comm ???";
                if (commManager && absCommIdx < commManager->GetItemCount())
                    commName = commManager->GetItemNameBuf(absCommIdx);
                bool commOpen = ImGui::TreeNodeEx(commName, ImGuiTreeNodeFlags_DefaultOpen);
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40);
                ImGui::TextDisabled("[%d]", absCommIdx);

                if (commOpen) {
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
                                GetActuatorField(cmd, f->field_path, val);
                                ImGui::Text("  %s = %.2f", f->name.c_str(), val);
                            }
                            if (visibleCnt == 0)
                                ImGui::TextDisabled("  (no visible fields)");
                            ImGui::TreePop();
                        }
                    }
                    ImGui::Unindent();
                    ImGui::TreePop();
                }
            }
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
                        case PinType::Enum: {
                            int iv = (int)gv.value;
                            if (iv >= 0 && iv < (int)gv.enumLabels.size())
                                ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.9f, 1.0f), "  %s = %s", gv.name.c_str(), gv.enumLabels[iv].c_str());
                            else
                                ImGui::Text("  %s = %d", gv.name.c_str(), iv);
                            break;
                        }
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

