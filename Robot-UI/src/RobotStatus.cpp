#include "RobotStatus.h"

RobotStatus::RobotStatus()
{
    m_CurrentCommand = std::make_shared<const ActuatorData>();
    WL_INFO_TAG("ROBOT_STATUS", "RobotStatus created");
}

void RobotStatus::ApplyFromRobotComponent(const RobotComponent& comp)
{
    const auto& modes = comp.GetModes();
    int idx = comp.GetActiveModeIndex();

    if (modes.empty())
    {
        // No modes configured yet — expected during early init, silently skip
        return;
    }

    if (idx < 0 || idx >= (int)modes.size())
    {
        WL_WARN_TAG("ROBOT_STATUS", "ApplyFromRobotComponent: invalid mode index {} (size={})", idx, modes.size());
        return;
    }

    const auto& mode = modes[idx];

    m_AppliedActuator    = mode.actuator_config;
    m_AppliedSendConfig  = mode.protocol_send;
    m_AppliedRecvConfig  = mode.protocol_receive;
    m_ActiveModeName     = mode.name;
    m_HasTemperature     = mode.has_temperature;
    m_HasHumidity        = mode.has_humidity;
    m_HasDepth           = mode.has_depth;

    WL_INFO_TAG("ROBOT_STATUS", "Applied config from mode: {}", m_ActiveModeName);
}

void RobotStatus::UpdateCommandData(std::shared_ptr<const ActuatorData> cmd)
{
    if (cmd)
        m_CurrentCommand = std::move(cmd);
}

void RobotStatus::UpdateSensorData(const SensorData& sensor, bool valid)
{
    m_CurrentSensor = sensor;
    m_SensorValid   = valid;
}

void RobotStatus::DrawWindow(bool* p_open, RobotCommManager* commManager)
{
    if (!ImGui::Begin("Robot Status", p_open))
    {
        ImGui::End();
        return;
    }

    ImGui::Separator();

    // 无锁读取：获取当前命令快照
    auto cmd = m_CurrentCommand;
    if (!cmd) cmd = std::make_shared<const ActuatorData>();

    // === Actuator — 按分组折叠显示 ===
    if (ImGui::CollapsingHeader("Actuator", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const auto& sendCfg = m_AppliedSendConfig;
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
        const auto& recvCfg = m_AppliedRecvConfig;
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
