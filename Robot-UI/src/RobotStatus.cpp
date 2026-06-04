#include "RobotStatus.h"
#include "GamepadMapper.h"
#include "LiveStreamManager.h"
#include "NodeGraphManager.h"
#include "RobotCommManager.h"

RobotStatus::RobotStatus()
{
    m_RobotAPI = std::make_shared<HardwareInterface>();
    m_CurrentCommand = std::make_shared<const ActuatorConfig>();
    m_GraphEvaluator = std::make_unique<NodeGraph>();
    WL_INFO_TAG("ROBOT_STATUS", "RobotStatus created (with headless graph evaluator)");
}

// ---- 同步 ActiveNodeGraph 到求值器 ----
void RobotStatus::SyncActiveNodeGraph()
{
    if (!m_NodeGraphManager) return;
    if (m_ActiveNodeGraphIdx < 0 || m_ActiveNodeGraphIdx >= m_NodeGraphManager->GetItemCount()) return;

    // 从 NodeGraphManager 的活跃项中获取图数据 YAML，加载到求值器
    std::string yaml = m_NodeGraphManager->GetGraphYamlForIndex(m_ActiveNodeGraphIdx);
    if (yaml.empty()) {
        WL_WARN_TAG("ROBOT_STATUS", "SyncActiveNodeGraph: no graph data for index {}", m_ActiveNodeGraphIdx);
        return;
    }

    std::unique_lock<std::shared_mutex> lock(m_StatusMutex);
    if (m_GraphEvaluator->LoadGraphData(yaml))
        WL_INFO_TAG("ROBOT_STATUS", "Synced graph from active NodeGraph #{} ({})",
                    m_ActiveNodeGraphIdx, m_NodeGraphManager->GetItemNameBuf(m_ActiveNodeGraphIdx));
    else
        WL_WARN_TAG("ROBOT_STATUS", "Failed to parse graph from active NodeGraph #{}",
                    m_ActiveNodeGraphIdx);
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
    if (yaml.empty()) {
        WL_WARN_TAG("ROBOT_STATUS", "SyncFromManagerSelected: no graph data at index {}", selIdx);
        return;
    }

    // 同时更新 RobotStatus 自己的索引
    m_ActiveNodeGraphIdx = selIdx;

    std::unique_lock<std::shared_mutex> lock(m_StatusMutex);
    if (m_GraphEvaluator->LoadGraphData(yaml))
        WL_INFO_TAG("ROBOT_STATUS", "Synced graph from Manager selected #{} ({})",
                    selIdx, m_NodeGraphManager->GetItemNameBuf(selIdx));
    else
        WL_WARN_TAG("ROBOT_STATUS", "Failed to parse graph from Manager selected #{}", selIdx);
}

RobotStatus::~RobotStatus()
{
    Unlink();
}

// ---- 活跃模式管理（写锁） ----

void RobotStatus::SetActiveMode(const RobotMode* item)
{
    std::unique_lock<std::shared_mutex> lock(m_StatusMutex);
    m_ActiveMode = item;
    if (item)
        WL_INFO_TAG("ROBOT_STATUS", "Selected item set to: {}", item->name);
    else
        WL_INFO_TAG("ROBOT_STATUS", "Selected item cleared");
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
    if (!m_GraphEvaluator || !m_ActiveMode) {
        if (m_GraphEvaluator) m_GraphEvaluator->Clear();
        return;
    }

    // 优先从 node_graph_pairs 查找
    auto it = m_ActiveMode->node_graph_pairs.find(gpMapperName);
    if (it != m_ActiveMode->node_graph_pairs.end()) {
        if (m_GraphEvaluator->LoadGraphData(it->second))
            WL_INFO_TAG("ROBOT_STATUS", "Loaded graph for item '{}' + gamepad '{}'",
                        m_ActiveMode->name, gpMapperName);
        else
            WL_WARN_TAG("ROBOT_STATUS", "Failed to parse graph for item '{}' + gamepad '{}'",
                        m_ActiveMode->name, gpMapperName);
    } else if (!m_ActiveMode->node_graph.empty()) {
        // 兼容旧版：使用 node_graph 字段
        m_GraphEvaluator->LoadGraphData(m_ActiveMode->node_graph);
        WL_INFO_TAG("ROBOT_STATUS", "Loaded legacy graph for item '{}'", m_ActiveMode->name);
    } else {
        m_GraphEvaluator->Clear();
        WL_INFO_TAG("ROBOT_STATUS", "No graph for item '{}' + gamepad '{}', cleared",
                    m_ActiveMode->name, gpMapperName);
    }
}

// ---- 节点图求值（读锁） ----

void RobotStatus::EvaluateIntoActuator(const std::map<std::string, float>& keyValues, ActuatorConfig& data)
{
    std::shared_lock lock(m_StatusMutex);
    if (m_GraphEvaluator)
        m_GraphEvaluator->EvaluateIntoActuator(keyValues, data);
}

// ---- 外联 getter（读锁） ----

bool RobotStatus::HasGraphEvaluator()    const { std::shared_lock lock(m_StatusMutex); return m_GraphEvaluator != nullptr; }
bool RobotStatus::HasActiveMode()        const { std::shared_lock lock(m_StatusMutex); return m_ActiveMode != nullptr; }
const RobotMode* RobotStatus::GetActiveModePtr() const { std::shared_lock lock(m_StatusMutex); return m_ActiveMode; }
GamepadMapper* RobotStatus::GetActiveGamepadPtr() const { std::shared_lock lock(m_StatusMutex); return m_ActiveGamepad; }
const std::string RobotStatus::GetActiveModeName() const { std::shared_lock lock(m_StatusMutex); return m_ActiveMode ? std::string(m_ActiveMode->name) : ""; }

const ActuatorConfig&        RobotStatus::GetAppliedActuator()   const { std::shared_lock lock(m_StatusMutex); return m_ActiveMode->actuator_config; }
const ProtocolSendConfig&    RobotStatus::GetAppliedSendConfig() const { std::shared_lock lock(m_StatusMutex); return m_ActiveMode->protocol_send; }
const ProtocolReceiveConfig& RobotStatus::GetAppliedRecvConfig() const { std::shared_lock lock(m_StatusMutex); return m_ActiveMode->protocol_receive; }
const SensorConfig&          RobotStatus::GetSensorConfig()      const { std::shared_lock lock(m_StatusMutex); return m_ActiveMode->sensor_config; }
bool RobotStatus::HasTemperature() const { std::shared_lock lock(m_StatusMutex); return m_ActiveMode && m_ActiveMode->sensor_config.has_temperature; }
bool RobotStatus::HasHumidity()    const { std::shared_lock lock(m_StatusMutex); return m_ActiveMode && m_ActiveMode->sensor_config.has_humidity; }
bool RobotStatus::HasDepth()       const { std::shared_lock lock(m_StatusMutex); return m_ActiveMode && m_ActiveMode->sensor_config.has_depth; }

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

// ---- 连接控制 ---- 
bool RobotStatus::Link(const RobotCommConfig& cfg)
{
    Unlink();  // 断开旧连接
    WL_INFO_TAG("ROBOT_STATUS", "Linking to {}:{} (local: {})...", cfg.host_ip, cfg.remote_port, cfg.local_port);

    bool ok = m_RobotAPI->Initialize(cfg.host_ip, cfg.remote_port, cfg.local_port);
    if (ok) {
        m_IsLinked = true;
        WL_INFO_TAG("ROBOT_STATUS", "Linked successfully: {} ({})", cfg.name, cfg.host_ip);
    } else {
        WL_ERROR_TAG("ROBOT_STATUS", "Link failed: {} ({})", cfg.name, cfg.host_ip);
    }
    return ok;
}

void RobotStatus::Unlink()
{
    if (m_IsLinked)
        WL_INFO_TAG("ROBOT_STATUS", "Unlinked");
    m_IsLinked = false;
}

void RobotStatus::SendActuatorData(const ActuatorConfig& data)
{
    if (m_IsLinked)
        m_RobotAPI->SendActuatorData(data);
}

SensorData RobotStatus::GetSensorData()
{
    if (m_IsLinked)
        return m_RobotAPI->GetSensorData();
    SensorData d; d.is_valid = false; return d;
}

void RobotStatus::DrawWindow(bool* p_open, RobotCommManager* commManager,
                              LiveStreamManager* liveStreamMgr,
                              NodeGraphManager* nodeGraphMgr)
{
    // 缓存 Manager 指针（每帧刷新，供 SyncActiveNodeGraph 等使用）
    m_NodeGraphManager = nodeGraphMgr;
    m_RobotCommManager = commManager;

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
                        SyncActiveNodeGraph();  // 切换 NodeGraph 时立即同步到求值器
                    }
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    // ---- Apply & Connect 按钮 ----
    if (commManager && commManager->GetItemCount() > 0)
    {
        if (m_ActiveCommIdx >= commManager->GetItemCount())
            m_ActiveCommIdx = 0;

        ImGui::Spacing();
        bool linked = m_IsLinked;
        ImVec4 statusColor = linked ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
        ImGui::TextColored(statusColor, "Status: %s", linked ? "Linked" : "Disconnected");

        ImGui::SetNextItemWidth(-1);
        const char* preview = commManager->GetItemNameBuf(m_ActiveCommIdx);
        if (ImGui::BeginCombo("##CommActive", preview))
        {
            for (int i = 0; i < commManager->GetItemCount(); ++i)
            {
                bool sel = (i == m_ActiveCommIdx);
                if (ImGui::Selectable(commManager->GetItemNameBuf(i), sel)) {
                    if (i != m_ActiveCommIdx) {
                        m_ActiveCommIdx = i;
                        // 切换 Comm 时若已连接则自动重连
                        if (m_IsLinked) {
                            auto items = commManager->GetAllItems();
                            if (i >= 0 && i < (int)items.size())
                                Link(items[i]);
                        }
                    }
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (!linked)
        {
            if (ImGui::Button("Connect", ImVec2(-1, 0)))
            {
                auto items = commManager->GetAllItems();
                if (m_ActiveCommIdx >= 0 && m_ActiveCommIdx < (int)items.size())
                    Link(items[m_ActiveCommIdx]);
            }
        }
        else
        {
            if (ImGui::Button("Disconnect", ImVec2(-1, 0)))
                Unlink();
        }
    }

    ImGui::Separator();

    // 快照：在共享锁下获取所有需要的数据，然后用局部变量绘制
    const RobotMode* item = nullptr;
    std::shared_ptr<const ActuatorConfig> cmd;
    {
        std::shared_lock lock(m_StatusMutex);
        item = m_ActiveMode;
        cmd  = m_CurrentCommand;
    }
    if (!cmd) cmd = std::make_shared<const ActuatorConfig>();

    if (!item) { ImGui::TextDisabled("No selected item"); ImGui::End(); return; }

    // === Actuator — 按分组折叠显示 ===
    if (ImGui::CollapsingHeader("Actuator", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const auto& sendCfg = item->protocol_send;
        if (sendCfg.fields.empty())
        {
            ImGui::TextDisabled("  No send fields configured");
        }
        else
        {
            std::map<std::string, std::vector<const SendField*>> groups;
            for (const auto& f : sendCfg.fields)
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

                        float val = 0.0f;
                        if (f->field_path == "motion.x")        val = (float)cmd->motion.x;
                        else if (f->field_path == "motion.y")   val = (float)cmd->motion.y;
                        else if (f->field_path == "motion.z")   val = (float)cmd->motion.z;
                        else if (f->field_path == "motion.rx")  val = (float)cmd->motion.rx;
                        else if (f->field_path == "motion.ry")  val = (float)cmd->motion.ry;
                        else if (f->field_path == "motion.rz")  val = (float)cmd->motion.rz;
                        else if (f->field_path.find("brushlessmotor.") == 0)
                        {
                            auto dot = f->field_path.find('.', 15);
                            std::string idStr = f->field_path.substr(15, dot - 15);
                            int mid = atoi(idStr.c_str());
                            std::string sub = f->field_path.substr(dot + 1);
                            if (cmd->brushlessmotor.count(mid))
                            {
                                if (sub == "target_speed")
                                    val = (float)cmd->brushlessmotor.at(mid).target_speed.value;
                            }
                        }
                        else if (f->field_path.find("servo.") == 0)
                        {
                            auto dot = f->field_path.find('.', 6);
                            std::string idStr = f->field_path.substr(6, dot - 6);
                            int sid = atoi(idStr.c_str());
                            if (cmd->servo.count(sid))
                                val = (float)cmd->servo.at(sid).angle.value;
                        }

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

    // === Sensor — 按分组折叠显示 ===
    if (ImGui::CollapsingHeader("Sensor", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const auto& recvCfg = item->protocol_receive;
        if (recvCfg.fields.empty())
        {
            ImGui::TextDisabled("  No receive fields configured");
        }
        else
        {
            std::map<std::string, std::vector<const ReceiveField*>> groups;
            for (const auto& f : recvCfg.fields)
                groups[f.group.empty() ? "Default" : f.group].push_back(&f);

            ImGui::Indent();
            for (const auto& g : groups)
            {
                if (ImGui::TreeNodeEx(g.first.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    int visibleCnt = 0;
                    SensorData sensorData;
                    bool hasData = false;
                    if (m_IsLinked)
                    {
                        sensorData = GetSensorData();
                        hasData = sensorData.is_valid;
                    }

                    for (const auto* f : g.second)
                    {
                        if (!f->visible) continue;
                        ++visibleCnt;

                        float val = 0.0f;
                        if (hasData)
                        {
                            if (f->field_path == "temperature.value")
                                val = (float)sensorData.temperature.value;
                            else if (f->field_path == "humidity.value")
                                val = (float)sensorData.humidity.value;
                            else if (f->field_path == "depth.value")
                                val = (float)sensorData.depth.value;
                        }

                        if (hasData)
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

    ImGui::End();
}

