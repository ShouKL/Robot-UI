#include "Robot_UI.h"
#include "Robot_API/hardware_interface.h"
#include "Walnut/EntryPoint.h"
#include "ConfigSerializer.h"
#include <imgui_node_editor.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <commdlg.h>   // GetOpenFileNameA / GetSaveFileNameA

// 获取 exe 所在目录（含尾部反斜杠），作为文件对话框默认路径
static std::string GetExeDir()
{
    char exePath[MAX_PATH] = "";
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    size_t pos = std::string(exePath).find_last_of("\\/");
    if (pos != std::string::npos)
        return std::string(exePath).substr(0, pos + 1);
    return "";
}

Robot_UI_Layer::Robot_UI_Layer()
    : m_AboutOpen(false), m_OptionOpen(false),
    m_SimulationOpen(false), m_LiveStreamerOpen(true), m_RobotStatusOpen(true), m_NodeEditorOpen(false)
{
    m_OptionPanel = std::make_unique<OptionPanel>();
    m_StreamManager = std::make_unique<StreamManager>();
    m_RobotCommManager = std::make_unique<RobotCommManager>();
    if (m_OptionPanel && m_OptionPanel->GetRobotConfig())
        m_RobotCommManager->SetRobotConfig(m_OptionPanel->GetRobotConfig().get());

    // NodeEditor creates its own ed::EditorContext internally
    m_NodeEditor = std::make_unique<NodeEditor>();
    m_ThrustCurveEditor = std::make_unique<ThrustCurveEditor>();

    // NodeEditor 模式切换回调 → 同步 GamepadMapper
    m_NodeEditor->SetModeSwitchCallback([this](int idx) {
        if (m_OptionPanel && m_OptionPanel->GetGamepadMapper())
            m_OptionPanel->GetGamepadMapper()->SetActiveModeByIndex(idx);
    });

    m_Running = true;
    m_CurrentCommand.store(std::make_shared<const ActuatorData>(), std::memory_order_relaxed);
    m_GamepadThread = std::thread(&Robot_UI_Layer::GamepadRoutine, this);

    // 启动时自动加载 exe 同目录下的 default_config.rbt
    std::string defaultPath = GetExeDir() + "default_config.rbt";
    LoadConfigFile(defaultPath);
}

Robot_UI_Layer::~Robot_UI_Layer()
{
    m_Running = false;
    if (m_GamepadThread.joinable())
    {
        m_GamepadThread.join();
    }
}

// ==================== File 操作 ====================

static std::string Win32OpenFileDialog(const char* filter)
{
    HWND hwnd = GetActiveWindow();
    char filePath[MAX_PATH] = "";
    std::string exeDir = GetExeDir();  // 必须存为局部变量，否则 c_str() 悬空
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = sizeof(filePath);
    ofn.lpstrInitialDir = exeDir.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn))
        return filePath;
    return "";
}

static std::string Win32SaveFileDialog(const char* filter, const char* defaultExt)
{
    HWND hwnd = GetActiveWindow();
    char filePath[MAX_PATH] = "";
    std::string exeDir = GetExeDir();  // 必须存为局部变量，否则 c_str() 悬空
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = sizeof(filePath);
    ofn.lpstrDefExt = defaultExt;
    ofn.lpstrInitialDir = exeDir.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (GetSaveFileNameA(&ofn))
        return filePath;
    return "";
}

void Robot_UI_Layer::FileOpen()
{
    std::string path = Win32OpenFileDialog("Robot UI Config (*.rbt)\0*.rbt\0All Files (*.*)\0*.*\0");
    if (path.empty()) return;

    std::vector<StreamConfig> streams;
    UIState uiState;
    std::string error;
    bool ok = ConfigSerializer::Load(
        path,
        *m_OptionPanel->GetRobotConfig(),
        *m_OptionPanel->GetGamepadMapper(),
        *m_OptionPanel->GetImGuiStyleManager(),
        streams,
        uiState,
        &m_ThrustCurveEditor->GetCurve(),
        &error);

    HWND hwnd = GetActiveWindow();
    if (!ok)
    {
        MessageBoxA(hwnd, error.c_str(), "Load Failed", MB_OK | MB_ICONWARNING);
        return;
    }

    m_CurrentSavePath = path;
    if (m_RobotCommManager) m_RobotCommManager->Disconnect();

    // 加载当前活跃模式的节点图
    if (m_NodeEditor && m_OptionPanel->GetRobotConfig())
    {
        auto& activeModes = m_OptionPanel->GetRobotConfig()->GetActiveModes();
        int idx = m_OptionPanel->GetRobotConfig()->GetActiveModeIndex();
        if (idx >= 0 && idx < (int)activeModes.size())
            m_NodeEditor->LoadGraphYaml(activeModes[idx].node_graph);
    }

    MessageBoxA(hwnd, ("Configuration loaded from:\n" + path).c_str(),
                "Load Success", MB_OK | MB_ICONINFORMATION);
}

void Robot_UI_Layer::FileSave()
{
    if (m_CurrentSavePath.empty())
    {
        FileSaveAs();
        return;
    }

    // 将当前节点图序列化到活跃 RobotMode
    if (m_NodeEditor && m_OptionPanel->GetRobotConfig())
    {
        auto& activeModes = m_OptionPanel->GetRobotConfig()->GetActiveModes();
        int idx = m_OptionPanel->GetRobotConfig()->GetActiveModeIndex();
        if (idx >= 0 && idx < (int)activeModes.size())
            activeModes[idx].node_graph = m_NodeEditor->GetGraphYaml();
    }

    std::vector<StreamConfig> streams;
    if (m_StreamManager)
        streams = m_StreamManager->GetAllStreamConfigs();

    UIState uiState;
    std::string error;
    if (!ConfigSerializer::Save(m_CurrentSavePath,
                                *m_OptionPanel->GetRobotConfig(),
                                *m_OptionPanel->GetGamepadMapper(),
                                *m_OptionPanel->GetImGuiStyleManager(),
                                streams, uiState,
                                &m_ThrustCurveEditor->GetCurve(),
                                &error))
    {
        MessageBoxA(GetActiveWindow(), error.c_str(), "Save Failed", MB_OK | MB_ICONWARNING);
    }
}

void Robot_UI_Layer::FileSaveAs()
{
    std::string path = Win32SaveFileDialog("Robot UI Config (*.rbt)\0*.rbt\0All Files (*.*)\0*.*\0", "rbt");
    if (path.empty()) return;

    m_CurrentSavePath = path;
    FileSave();

    MessageBoxA(GetActiveWindow(), ("Configuration saved to:\n" + path).c_str(),
                "Save Success", MB_OK | MB_ICONINFORMATION);
}

void Robot_UI_Layer::LoadConfigFile(const std::string& path)
{
    std::vector<StreamConfig> streams;
    UIState uiState;
    std::string error;
    if (!ConfigSerializer::Load(
            path,
            *m_OptionPanel->GetRobotConfig(),
            *m_OptionPanel->GetGamepadMapper(),
            *m_OptionPanel->GetImGuiStyleManager(),
            streams, uiState,
            &m_ThrustCurveEditor->GetCurve(),
            &error))
    {
        return;  // 静默失败（文件不存在或格式错误）
    }

    m_CurrentSavePath = path;
    if (m_RobotCommManager) m_RobotCommManager->Disconnect();

    // 恢复 UI 状态
    ApplyUIState(uiState);

    // 加载当前活跃模式的节点图
    if (m_NodeEditor && m_OptionPanel->GetRobotConfig())
    {
        auto& activeModes = m_OptionPanel->GetRobotConfig()->GetActiveModes();
        int idx = m_OptionPanel->GetRobotConfig()->GetActiveModeIndex();
        if (idx >= 0 && idx < (int)activeModes.size())
            m_NodeEditor->LoadGraphYaml(activeModes[idx].node_graph);
    }
}

UIState Robot_UI_Layer::CollectUIState() const
{
    return {};
}

void Robot_UI_Layer::ApplyUIState(const UIState&)
{
}

void Robot_UI_Layer::GamepadRoutine()
{
    bool lastModeToggle = false;
    while (m_Running)
    {
        // ===== 始终收集手柄键值并评估节点图（使 NodeEditor "Input Keys" 侧边栏始终显示） =====
        if (m_OptionPanel && m_OptionPanel->GetGamepadMapper() && m_NodeEditor)
        {
            std::map<std::string, float> keyValues;
            // 线程安全快照：GetActiveModeBoundKeyNames() 内部加锁
            auto boundKeys = m_OptionPanel->GetGamepadMapper()->GetActiveModeBoundKeyNames();
            for (const auto& keyName : boundKeys) {
                keyValues[keyName] = m_OptionPanel->GetGamepadMapper()->GetKeyValue(keyName);
            }
            m_NodeEditor->EvaluateGraph(keyValues);
        }

        if (m_RobotCommManager && m_RobotCommManager->IsConnected())
        {
            ActuatorData data;

            if (m_OptionPanel->GetRobotConfig()) {
                data = m_OptionPanel->GetRobotConfig()->GetActiveActuatorConfig();
            }

            if (m_OptionPanel->GetGamepadMapper()) {
                float modeToggleVal = m_OptionPanel->GetGamepadMapper()->GetKeyValue("Mode Toggle");
                bool currentModeToggle = modeToggleVal > 0.5f;
                if (currentModeToggle && !lastModeToggle) {
                    if (m_OptionPanel->GetRobotConfig()) {
                        m_OptionPanel->GetRobotConfig()->NextActiveMode();
                        data = m_OptionPanel->GetRobotConfig()->GetActiveActuatorConfig();

                        // 切换到新模式时，同步手柄映射预设和节点图
                        auto& active_modes = m_OptionPanel->GetRobotConfig()->GetActiveModes();
                        int active_idx = m_OptionPanel->GetRobotConfig()->GetActiveModeIndex();
                        if (!active_modes.empty() && active_idx < (int)active_modes.size()) {
                            std::string Mode = active_modes[active_idx].gamepad_mapping_Mode;
                            if (m_OptionPanel->GetGamepadMapper()->ModeExists(Mode)) {
                                m_OptionPanel->GetGamepadMapper()->SetActiveMode(Mode);
                            }
                            // 同步节点图
                            if (m_NodeEditor) {
                                m_NodeEditor->LoadGraphYaml(active_modes[active_idx].node_graph);
                            }
                        }
                    }
                }
                lastModeToggle = currentModeToggle;

                // 通过节点图求值：收集所有自定义键位的值，输入节点图，输出 ActuatorData
                if (m_NodeEditor && m_OptionPanel->GetGamepadMapper()) {
                    std::map<std::string, float> keyValues;
                    // 线程安全快照：GetActiveModeBoundKeyNames() 内部加锁
                    auto boundKeys = m_OptionPanel->GetGamepadMapper()->GetActiveModeBoundKeyNames();
                    for (const auto& keyName : boundKeys) {
                        keyValues[keyName] = m_OptionPanel->GetGamepadMapper()->GetKeyValue(keyName);
                    }

                    // 节点图求值并直接写入 ActuatorData
                    m_NodeEditor->EvaluateIntoActuator(keyValues, data);
                }
            }

            // 无锁发布：atomic store shared_ptr，UI 线程通过 load 获取一致快照
            m_CurrentCommand.store(std::make_shared<const ActuatorData>(data), std::memory_order_release);

            if (m_RobotCommManager->IsConnected()) {
                m_RobotCommManager->SendActuatorData(data);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20Hz
    }
}

void Robot_UI_Layer::OnUIRender()
{
    if (m_OptionPanel && m_OptionPanel->GetGamepadMapper()) {
        m_OptionPanel->GetGamepadMapper()->UpdateGamepadState();
    }

    if (m_StreamManager) {
        m_StreamManager->UpdateAll();
    }

    if (m_LiveStreamerOpen && m_StreamManager)
    {
        m_StreamManager->DrawUI("Live Streamer", &m_LiveStreamerOpen);
    }

    if (m_AboutOpen)
    {
        if (ImGui::Begin("About", nullptr, ImGuiWindowFlags_AlwaysVerticalScrollbar))
        {
            ImGui::Text("Robot UI v1.0");
            if (ImGui::Button("Close##1", ImVec2(-1, 0)))
            {
                m_AboutOpen = false;
            }
        }
        ImGui::End();
    }

    if (m_OptionOpen && m_OptionPanel)
    {
        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(ImVec2(displaySize.x * 0.85f, displaySize.y * 0.8f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(400, 300),
            ImVec2(displaySize.x, displaySize.y));
        if (ImGui::Begin("Option", nullptr, ImGuiWindowFlags_AlwaysVerticalScrollbar))
        {
            m_OptionPanel->DrawOptionPanel();

            float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

            if (ImGui::Button("Apply", ImVec2(buttonWidth, 0)))
            {
                if (m_OptionPanel->Apply()) {
                    if (m_RobotCommManager) m_RobotCommManager->Disconnect();
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Close##2", ImVec2(buttonWidth, 0)))
            {
                m_OptionOpen = false;
                m_OptionPanel->Revert();
            }
        }
        ImGui::End();
    }

    if (m_RobotStatusOpen)
    {
        if (ImGui::Begin("Robot Status", &m_RobotStatusOpen))
        {
            ImGui::Separator();
            // 无锁读取：一次 atomic load 获取整帧一致的命令快照
            auto cmd = m_CurrentCommand.load(std::memory_order_acquire);
            if (!cmd) cmd = std::make_shared<const ActuatorData>();

            // === Actuator — TreeNodes for hierarchy ===
            if (ImGui::CollapsingHeader("Actuator", ImGuiTreeNodeFlags_DefaultOpen)) {
                const auto& sendCfg = m_OptionPanel->GetRobotConfig()->GetActiveProtocolConfig();
                if (sendCfg.fields.empty()) {
                    ImGui::TextDisabled("  No send fields configured");
                } else {
                    std::map<std::string, std::vector<const SendField*>> groups;
                    for (const auto& f : sendCfg.fields)
                        groups[f.group.empty() ? "Default" : f.group].push_back(&f);
                    ImGui::Indent();
                    for (const auto& g : groups) {
                        if (ImGui::TreeNodeEx(g.first.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                            int visibleCnt = 0;
                            for (const auto* f : g.second) {
                                if (!f->visible) continue;
                                ++visibleCnt;
                                float val = 0.0f;
                                if (f->field_path == "motion.x")  val = (float)cmd->motion.x;
                                else if (f->field_path == "motion.y")  val = (float)cmd->motion.y;
                                else if (f->field_path == "motion.z")  val = (float)cmd->motion.z;
                                else if (f->field_path == "motion.rx") val = (float)cmd->motion.rx;
                                else if (f->field_path == "motion.ry") val = (float)cmd->motion.ry;
                                else if (f->field_path == "motion.rz") val = (float)cmd->motion.rz;
                                else if (f->field_path.find("brushlessmotor.") == 0) {
                                    auto dot = f->field_path.find('.', 15);
                                    std::string idStr = f->field_path.substr(15, dot - 15);
                                    int mid = atoi(idStr.c_str());
                                    std::string sub = f->field_path.substr(dot + 1);
                                    if (cmd->brushlessmotor.count(mid)) {
                                        if (sub == "target_speed") val = (float)cmd->brushlessmotor.at(mid).target_speed.value;
                                    }
                                }
                                else if (f->field_path.find("servo.") == 0) {
                                    auto dot = f->field_path.find('.', 6);
                                    std::string idStr = f->field_path.substr(6, dot - 6);
                                    int sid = atoi(idStr.c_str());
                                    if (cmd->servo.count(sid))
                                        val = (float)cmd->servo.at(sid).angle.value;
                                }
                                ImGui::Text("  %s = %.2f", f->name.c_str(), val);
                            }
                            if (visibleCnt == 0) ImGui::TextDisabled("  (no visible fields)");
                            ImGui::TreePop();
                        }
                    }
                    ImGui::Unindent();
                }
            }

            // === Sensor — TreeNodes with indent for hierarchy ===
            if (ImGui::CollapsingHeader("Sensor", ImGuiTreeNodeFlags_DefaultOpen)) {
                const auto& recvCfg = m_OptionPanel->GetRobotConfig()->GetActiveProtocolReceiveConfig();
                if (recvCfg.fields.empty()) {
                    ImGui::TextDisabled("  No receive fields configured");
                } else {
                    std::map<std::string, std::vector<const ReceiveField*>> groups;
                    for (const auto& f : recvCfg.fields)
                        groups[f.group.empty() ? "Default" : f.group].push_back(&f);
                    ImGui::Indent();
                    for (const auto& g : groups) {
                        if (ImGui::TreeNodeEx(g.first.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                            int visibleCnt = 0;
                            SensorData sensorData;
                            bool hasData = false;
                            if (m_RobotCommManager->IsConnected()) {
                                sensorData = m_RobotCommManager->GetSensorData();
                                hasData = sensorData.is_valid;
                            }
                            for (const auto* f : g.second) {
                                if (!f->visible) continue;
                                ++visibleCnt;
                                float val = 0.0f;
                                if (hasData) {
                                    if (f->field_path == "temperature.value") val = (float)sensorData.temperature.value;
                                    else if (f->field_path == "humidity.value") val = (float)sensorData.humidity.value;
                                    else if (f->field_path == "depth.value") val = (float)sensorData.depth.value;
                                }
                                if (hasData)
                                    ImGui::Text("  %s = %.2f", f->name.c_str(), val);
                                else
                                    ImGui::Text("  %s = --", f->name.c_str());
                            }
                            if (visibleCnt == 0) ImGui::TextDisabled("  (no visible fields)");
                            ImGui::TreePop();
                        }
                    }
                    ImGui::Unindent();
                }
            }
        ImGui::End();
        }
    }

    // 节点编辑器窗口
    if (m_NodeEditorOpen && m_NodeEditor)
    {
        // 同步可用键位名（来自 GamepadMapper）
        if (m_OptionPanel && m_OptionPanel->GetGamepadMapper())
        {
            const auto& modes = m_OptionPanel->GetGamepadMapper()->GetModes();
            int activeIdx = m_OptionPanel->GetGamepadMapper()->GetActiveModeIndex();

            // 同步模式名列表（供 NodeEditor 顶部下拉切换）
            std::vector<std::string> modeNames;
            for (const auto& m : modes)
                modeNames.push_back(m.name);
            m_NodeEditor->SetModeNames(modeNames, activeIdx);

            // 同步当前模式的键位名
            std::vector<std::string> keyNames;
            std::set<std::string> analogKeys;
            if (activeIdx >= 0 && activeIdx < (int)modes.size())
            {
                for (const auto& mapping : modes[activeIdx].mappings) {
                    keyNames.push_back(mapping.key_name);
                    if (mapping.is_analog) {
                        analogKeys.insert(mapping.key_name);
                    }
                }
            }
            m_NodeEditor->SetAvailableKeyNames(keyNames);
            m_NodeEditor->SetAnalogKeys(analogKeys);
        }

        // 同步可用输出目标（来自 protocol send fields，排除 fix）
        if (m_OptionPanel && m_OptionPanel->GetRobotConfig())
        {
            auto& cfg = m_OptionPanel->GetRobotConfig();
            const auto& proto = cfg->GetActiveProtocolConfig();
            auto data = cfg->GetActiveActuatorConfig();
            auto targets = BuildOutputTargetsFromProtocol(proto, data);
            m_NodeEditor->SetAvailableOutputTargets(targets);

            // 同步当前 actuator field 值（field_path → value）
            std::map<std::string, double> fieldVals;
            for (const auto& t : targets) {
                double val = 0.0;
                if (GetActuatorField(data, t.field_path, val))
                    fieldVals[t.field_path] = val;
            }
            m_NodeEditor->SetFieldValues(fieldVals);
        }

        m_NodeEditorOpen = m_NodeEditor->Draw();
    }

    // 推力曲线编辑器窗口
    if (m_ThrustCurveEditorOpen && m_ThrustCurveEditor)
    {
        m_ThrustCurveEditor->Draw();
        m_ThrustCurveEditorOpen = m_ThrustCurveEditor->IsOpen();
    }

    // RobotComm Configuration
    if (m_RobotCommManager)
    {
        m_RobotCommManager->DrawUI("Robot Comm", &m_RobotCommOpen);
    }

    // Protocol Field Configuration 子窗口
    if (m_OptionPanel && m_OptionPanel->GetRobotConfig())
    {
        m_OptionPanel->GetRobotConfig()->DrawProtocolConfigWindow();
    }
}

void Robot_UI_Layer::ShowNodeEditor()
{
    m_NodeEditorOpen = true;
}

void Robot_UI_Layer::ShowThrustCurveEditor()
{
    m_ThrustCurveEditor->Open();
    m_ThrustCurveEditorOpen = true;
}

void Robot_UI_Layer::ShowRobotCommConfig()
{
    if (m_RobotCommManager)
        m_RobotCommManager->OpenConfigWindow();
}

void Robot_UI_Layer::SyncNodeEditorModes()
{
    if (m_NodeEditor && m_OptionPanel && m_OptionPanel->GetRobotConfig())
    {
        auto& activeModes = m_OptionPanel->GetRobotConfig()->GetActiveModes();
        int idx = m_OptionPanel->GetRobotConfig()->GetActiveModeIndex();
        if (idx >= 0 && idx < (int)activeModes.size())
            m_NodeEditor->LoadGraphYaml(activeModes[idx].node_graph);
    }
}

// --- Entry Point ---
Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
    Walnut::ApplicationSpecification spec;
    spec.Name = "Robot UI";
    spec.CustomTitlebar = true;

    Walnut::Application* app = new Walnut::Application(spec);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

    std::shared_ptr<Robot_UI_Layer> uiLayer = std::make_shared<Robot_UI_Layer>();
    app->PushLayer(uiLayer);

    app->SetMenubarCallback([app, uiLayer]()
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open"))
                {
                    uiLayer->FileOpen();
                }
                if (ImGui::MenuItem("Save"))
                {
                    uiLayer->FileSave();
                }
                if (ImGui::MenuItem("Save As"))
                {
                    uiLayer->FileSaveAs();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit"))
                {
                    app->Close();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Editor"))
            {
                if (ImGui::MenuItem("Node Editor"))
                {
                    uiLayer->ShowNodeEditor();
                }
                if (ImGui::MenuItem("Thrust Curve Editor"))
                {
                    uiLayer->ShowThrustCurveEditor();
                }
                ImGui::EndMenu();
            }
                

            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Live Streamer", nullptr, &uiLayer->GetLiveStreamerOpen());
                ImGui::MenuItem("Robot Comm", nullptr, &uiLayer->GetShowRobotComm());
                ImGui::MenuItem("Robot Status", nullptr, &uiLayer->GetShowRobotStatus());
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Tools"))
            {
                if (ImGui::MenuItem("Option"))
                {
                    uiLayer->ShowOption();
                }
                if (ImGui::MenuItem("About"))
                {
                    uiLayer->ShowAbout();
                }
                ImGui::EndMenu();
            }
        });

    return app;
}