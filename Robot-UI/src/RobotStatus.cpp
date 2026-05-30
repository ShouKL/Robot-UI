#include "RobotStatus.h"

RobotStatus::RobotStatus()
{
    m_CurrentCommand = std::make_shared<const ActuatorConfig>();
    m_GraphEvaluator = std::make_unique<NodeGraph>();
    WL_INFO_TAG("ROBOT_STATUS", "RobotStatus created (with headless graph evaluator)");
}

// ---- 活跃模式管理（写锁） ----

void RobotStatus::SetActiveMode(const RobotMode* mode)
{
    std::unique_lock lock(m_StatusMutex);
    m_ActiveMode = mode;
    if (mode)
        WL_INFO_TAG("ROBOT_STATUS", "Active mode set to: {}", mode->name);
    else
        WL_INFO_TAG("ROBOT_STATUS", "Active mode cleared");
}

void RobotStatus::LoadGraph(const std::string& gamepadModeName)
{
    std::unique_lock lock(m_StatusMutex);
    if (!m_GraphEvaluator || !m_ActiveMode) {
        if (m_GraphEvaluator) m_GraphEvaluator->Clear();
        return;
    }

    // 优先从 node_graph_pairs 查找
    auto it = m_ActiveMode->node_graph_pairs.find(gamepadModeName);
    if (it != m_ActiveMode->node_graph_pairs.end()) {
        if (m_GraphEvaluator->LoadGraphData(it->second))
            WL_INFO_TAG("ROBOT_STATUS", "Loaded graph for mode '{}' + gamepad '{}'",
                        m_ActiveMode->name, gamepadModeName);
        else
            WL_WARN_TAG("ROBOT_STATUS", "Failed to parse graph for mode '{}' + gamepad '{}'",
                        m_ActiveMode->name, gamepadModeName);
    } else if (!m_ActiveMode->node_graph.empty()) {
        // 兼容旧版：使用 node_graph 字段
        m_GraphEvaluator->LoadGraphData(m_ActiveMode->node_graph);
        WL_INFO_TAG("ROBOT_STATUS", "Loaded legacy graph for mode '{}'", m_ActiveMode->name);
    } else {
        m_GraphEvaluator->Clear();
        WL_INFO_TAG("ROBOT_STATUS", "No graph for mode '{}' + gamepad '{}', cleared",
                    m_ActiveMode->name, gamepadModeName);
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

void RobotStatus::DrawWindow(bool* p_open, RobotCommManager* commManager)
{
    if (!ImGui::Begin("Robot Status", p_open))
    {
        ImGui::End();
        return;
    }

    ImGui::Separator();

    // 快照：在共享锁下获取所有需要的数据，然后用局部变量绘制
    const RobotMode* mode = nullptr;
    std::shared_ptr<const ActuatorConfig> cmd;
    {
        std::shared_lock lock(m_StatusMutex);
        mode = m_ActiveMode;
        cmd  = m_CurrentCommand;
    }
    if (!cmd) cmd = std::make_shared<const ActuatorConfig>();

    if (!mode) { ImGui::TextDisabled("No active mode"); ImGui::End(); return; }

    // === Actuator — 按分组折叠显示 ===
    if (ImGui::CollapsingHeader("Actuator", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const auto& sendCfg = mode->protocol_send;
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
        const auto& recvCfg = mode->protocol_receive;
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
                    if (commManager && commManager->IsConnected())
                    {
                        sensorData = commManager->GetSensorData();
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
