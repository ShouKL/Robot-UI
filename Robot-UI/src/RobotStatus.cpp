#include "RobotStatus.h"
#include "GamepadMapper.h"
#include "GamepadMapperManager.h"
#include "LiveStreamManager.h"
#include "NodeGraphManager.h"
#include "RobotCommManager.h"
#include "RobotComponentManager.h"
#include <cstring>
#include <yaml-cpp/yaml.h>

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

    std::string yaml = m_NodeGraphManager->GetGraphYamlForIndex(m_ActiveNodeGraphIdx);
    if (yaml.empty()) return;

    // Skip if same as last synced
    if (yaml == m_LastSyncedYaml) return;
    m_LastSyncedYaml = yaml;

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

// ---- 获取当前活跃 Comm 的发送频率 ----
int RobotStatus::GetSendFreqHz() const
{
    if (!m_RobotCommManager || m_ActiveCommIdx < 0) return 100;
    auto items = m_RobotCommManager->GetAllItems();
    if (m_ActiveCommIdx >= (int)items.size()) return 100;
    int hz = items[m_ActiveCommIdx].send_freq_hz;
    return hz > 0 ? hz : 100;
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
                    if (std::strcmp(c.component.name, modeName.c_str()) == 0) {
                        SetActiveMode(&c.component);
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

        // 推导 Comm
        if (m_RobotCommManager && root["comm_index"]) {
            int commIdx = root["comm_index"].as<int>();
            if (commIdx >= 0 && commIdx < m_RobotCommManager->GetItemCount()) {
                SetActiveCommIdx(commIdx);
                WL_INFO_TAG("ROBOT_STATUS", "Derived ActiveComm #{} from NodeGraph #{}", commIdx, m_ActiveNodeGraphIdx);
            }
        }
    }
    catch (const std::exception& e) {
        WL_WARN_TAG("ROBOT_STATUS", "DeriveActiveFromNodeGraph YAML parse failed: {}", e.what());
    }
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
    if (item) {
        WL_INFO_TAG("ROBOT_STATUS", "Selected item set to: {}", item->name);
    } else {
        WL_INFO_TAG("ROBOT_STATUS", "Selected item cleared");
    }
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
const std::vector<ProtocolSendConfig>& RobotStatus::GetAppliedSendConfig() const { std::shared_lock lock(m_StatusMutex); return m_ActiveMode->protocol_send; }
const std::vector<ProtocolReceiveConfig>& RobotStatus::GetAppliedRecvConfig() const { std::shared_lock lock(m_StatusMutex); return m_ActiveMode->protocol_receive; }
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

    bool ok = m_RobotAPI->Initialize(cfg.host_ip, cfg.remote_port, cfg.local_port, cfg.transport_type);
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
    m_LastSyncedProtocolItem = nullptr;
}

void RobotStatus::SendActuatorData(const ActuatorConfig& data)
{
    if (m_IsLinked) {
        m_RobotAPI->SendActuatorData(data);
        // 发送后检查连接是否已被硬件层标记为断开
        if (!m_RobotAPI->IsConnected()) {
            WL_ERROR_TAG("ROBOT_STATUS", "Connection lost during send — auto-unlinking");
            Unlink();
        }
    }
}

SensorData RobotStatus::GetSensorData()
{
    if (m_IsLinked) {
        SensorData d = m_RobotAPI->GetSensorData();
        // 接收后检查连接是否已被硬件层标记为断开
        if (!m_RobotAPI->IsConnected()) {
            WL_ERROR_TAG("ROBOT_STATUS", "Connection lost during recv — auto-unlinking");
            Unlink();
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

    // ---- Connect / Disconnect 按钮（Comm 由 NodeGraph 确定，不暴露下拉框）----
    if (commManager && commManager->GetItemCount() > 0)
    {
        if (m_ActiveCommIdx >= commManager->GetItemCount())
            m_ActiveCommIdx = 0;

        ImGui::Spacing();
        bool linked = m_IsLinked;
        ImVec4 statusColor = linked ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
        ImGui::TextColored(statusColor, "Status: %s", linked ? "Linked" : "Disconnected");

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

    // ---- 当前 Active 项一览 ----
    {
        ImGui::Spacing();
        ImGui::TextDisabled("Active Items:");

        const RobotMode* mode = nullptr;
        GamepadMapper* gp = nullptr;
        {
            std::shared_lock lock(m_StatusMutex);
            mode = m_ActiveMode;
            gp = m_ActiveGamepad;
        }

        ImGui::Text("  Component:    %s", mode ? mode->name : "(none)");
        ImGui::Text("  Gamepad:      %s", gp ? gp->name : "(none)");

        if (nodeGraphMgr && m_ActiveNodeGraphIdx < nodeGraphMgr->GetItemCount())
            ImGui::Text("  NodeGraph:    %s", nodeGraphMgr->GetItemNameBuf(m_ActiveNodeGraphIdx));
        else
            ImGui::Text("  NodeGraph:    (none)");

        if (liveStreamMgr && m_ActiveLiveStreamIdx < liveStreamMgr->GetItemCount())
            ImGui::Text("  LiveStream:   %s", liveStreamMgr->GetItemNameBuf(m_ActiveLiveStreamIdx));
        else
            ImGui::Text("  LiveStream:   (none)");

        if (commManager && m_ActiveCommIdx < commManager->GetItemCount())
            ImGui::Text("  Comm Config:  %s", commManager->GetItemNameBuf(m_ActiveCommIdx));
        else
            ImGui::Text("  Comm Config:  (none)");
    }

    ImGui::Separator();

    // 快照：在共享锁下获取所有需要的数据，然后用局部变量绘制
    const RobotMode* item = nullptr;
    std::shared_ptr<const ActuatorConfig> cmd;
    bool linked;
    {
        std::shared_lock lock(m_StatusMutex);
        item = m_ActiveMode;
        cmd  = m_CurrentCommand;
        linked = m_IsLinked;
    }
    // 未连接时，使用 RobotComponent 中配置的初始值；连接后使用实时求值结果
    if (!cmd || !linked)
        cmd = std::make_shared<const ActuatorConfig>(item ? item->actuator_config : ActuatorConfig{});

    if (!item) { ImGui::TextDisabled("No selected item"); ImGui::End(); return; }

    // === Send Control — 控制发送哪些数据帧 ===
    if (ImGui::CollapsingHeader("Send Control", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const auto& sendCfgs = item->protocol_send;
        if (sendCfgs.empty())
        {
            ImGui::TextDisabled("  No send frames configured");
        }
        else
        {
            ImGui::Indent();
            auto* modeMut = const_cast<RobotMode*>(item);

            // 当前活跃 item 变化时，自动推送协议配置到 HardwareInterface
            if (m_IsLinked && m_LastSyncedProtocolItem != item) {
                m_RobotAPI->SetProtocolConfig(sendCfgs);
                m_RobotAPI->SetProtocolReceiveConfig(item->protocol_receive);
                m_LastSyncedProtocolItem = item;
            }

            bool anyChanged = false;
            for (size_t i = 0; i < sendCfgs.size(); ++i)
            {
                auto& sc = modeMut->protocol_send[i];
                ImGui::PushID((int)i);
                if (ImGui::Checkbox("##sendEn", &sc.enabled))
                    anyChanged = true;
                ImGui::PopID();
                ImGui::SameLine();
                ImGui::TextColored(sc.enabled ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                    "%s  [0x%02X] %s (%zu fields)",
                    sc.name, sc.command_byte, sc.enabled ? "ON" : "OFF", sc.fields.size());
            }
            if (anyChanged && m_IsLinked)
                m_RobotAPI->SetProtocolConfig(modeMut->protocol_send);
            ImGui::Unindent();
        }
    }

    // === Actuator — 分组来自 protocol_send，值实时显示（连接时用求值结果，未连接用初始配置） ===
    if (ImGui::CollapsingHeader("Actuator", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const auto& sendCfgs = item->protocol_send;
        if (sendCfgs.empty())
        {
            ImGui::TextDisabled("  No send frames configured");
        }
        else
        {
            // 聚合所有 send frame 的字段（按 group 分组）
            std::map<std::string, std::vector<const SendField*>> groups;
            for (const auto& sc : sendCfgs)
                for (const auto& f : sc.fields)
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

    // === Sensor — 按分组折叠显示 ===
    if (ImGui::CollapsingHeader("Sensor", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const auto& recvCfgs = item->protocol_receive;
        if (recvCfgs.empty())
        {
            ImGui::TextDisabled("  No receive frames configured");
        }
        else
        {
            // 聚合所有接收帧的字段（按 group 分组）
            std::map<std::string, std::vector<const ReceiveField*>> groups;
            for (const auto& rc : recvCfgs)
                for (const auto& f : rc.fields)
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

    ImGui::End();
}

