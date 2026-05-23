#include "Robot_UI.h"
#include "Robot_API/ROV_API/hardware_interface.h"
#include "Walnut/EntryPoint.h"
#include "ConfigSerializer.h"
#include <imgui_node_editor.h>
#include <GLFW/glfw3.h>
#include <chrono>

Robot_UI_Layer::Robot_UI_Layer(std::shared_ptr<RobotAPI> robotAPI)
    : m_AboutOpen(false), m_OptionOpen(false),
    m_SimulationOpen(false), m_LiveStreamerOpen(true), m_RobotStatusOpen(true), m_NodeEditorOpen(false),
    m_RobotAPI(robotAPI)
{
    m_OptionPanel = std::make_unique<OptionPanel>();
    m_StreamManager = std::make_unique<StreamManager>();

    // NodeEditor creates its own ed::EditorContext internally
    m_NodeEditor = std::make_unique<NodeEditor>();

    m_Running = true;
    m_GamepadThread = std::thread(&Robot_UI_Layer::GamepadRoutine, this);

    // 启动时自动加载默认配置文件
    std::string defaultCfg = GetDefaultConfigPath();
    LoadConfigFile(defaultCfg);
}

Robot_UI_Layer::~Robot_UI_Layer()
{
    m_Running = false;
    if (m_GamepadThread.joinable())
    {
        m_GamepadThread.join();
    }
    // NodeEditor destructor destroys its ed::EditorContext
}

void Robot_UI_Layer::FileOpen()
{
    std::string path = ConfigSerializer::OpenFileDialog("Robot UI Config (*.rbt)\0*.rbt\0All Files (*.*)\0*.*\0");
    if (path.empty()) return;

    std::vector<StreamConfig> streams;
    std::string error;
    bool ok = ConfigSerializer::Load(
        path,
        *m_OptionPanel->GetRobotConfig(),
        *m_OptionPanel->GetGamepadMapper(),
        *m_OptionPanel->GetImGuiStyleManager(),
        streams,
        &error);

    HWND hwnd = GetActiveWindow();
    if (!ok)
    {
        MessageBoxA(hwnd, error.c_str(), "Load Failed", MB_OK | MB_ICONWARNING);
        return;
    }

    m_CurrentSavePath = path;
    m_IsConnected = false;

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
        m_CurrentSavePath = GetDefaultConfigPath();

    // 将当前节点图同步到 modes（ConfigSerializer::Save 读取 modes，非 active_modes）
    if (m_NodeEditor && m_OptionPanel->GetRobotConfig())
    {
        std::string graphYaml = m_NodeEditor->GetGraphYaml();

        auto& activeModes = m_OptionPanel->GetRobotConfig()->GetActiveModes();
        int idx = m_OptionPanel->GetRobotConfig()->GetActiveModeIndex();
        if (idx >= 0 && idx < (int)activeModes.size())
            activeModes[idx].node_graph = graphYaml;

        // Also sync to modes so ConfigSerializer::Save picks it up
        auto& modes = m_OptionPanel->GetRobotConfig()->GetModes();
        if (idx >= 0 && idx < (int)modes.size())
            modes[idx].node_graph = graphYaml;
    }

    std::vector<StreamConfig> streams;
    if (m_StreamManager)
        streams = m_StreamManager->GetAllStreamConfigs();

    std::string error;
    if (!ConfigSerializer::Save(m_CurrentSavePath,
                                *m_OptionPanel->GetRobotConfig(),
                                *m_OptionPanel->GetGamepadMapper(),
                                *m_OptionPanel->GetImGuiStyleManager(),
                                streams, &error))
    {
        MessageBoxA(GetActiveWindow(), error.c_str(), "Save Failed", MB_OK | MB_ICONWARNING);
    }
}

void Robot_UI_Layer::FileSaveAs()
{
    std::string path = ConfigSerializer::SaveFileDialog("Robot UI Config (*.rbt)\0*.rbt\0All Files (*.*)\0*.*\0", "rbt");
    if (path.empty()) return;

    m_CurrentSavePath = path;
    FileSave();

    MessageBoxA(GetActiveWindow(), ("Configuration saved to:\n" + path).c_str(),
                "Save Success", MB_OK | MB_ICONINFORMATION);
}

void Robot_UI_Layer::LoadConfigFile(const std::string& path)
{
    std::vector<StreamConfig> streams;
    std::string error;
    bool ok = ConfigSerializer::Load(
        path,
        *m_OptionPanel->GetRobotConfig(),
        *m_OptionPanel->GetGamepadMapper(),
        *m_OptionPanel->GetImGuiStyleManager(),
        streams,
        &error);

    if (!ok) return;  // 文件不存在或格式错误时静默跳过

    m_CurrentSavePath = path;
    m_IsConnected = false;

    if (m_NodeEditor && m_OptionPanel->GetRobotConfig())
    {
        auto& activeModes = m_OptionPanel->GetRobotConfig()->GetActiveModes();
        int idx = m_OptionPanel->GetRobotConfig()->GetActiveModeIndex();
        if (idx >= 0 && idx < (int)activeModes.size())
            m_NodeEditor->LoadGraphYaml(activeModes[idx].node_graph);
    }
}

std::string Robot_UI_Layer::GetDefaultConfigPath() const
{
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    std::string dir(exePath);
    size_t lastSlash = dir.find_last_of("\\/");
    if (lastSlash != std::string::npos)
        dir = dir.substr(0, lastSlash + 1);
    return dir + "RobotUI_Config.rbt";
}

void Robot_UI_Layer::ShowNodeEditor()
{
    m_NodeEditorOpen = true;
    SyncNodeEditorModes();
    m_NodeEditor->OnOpen();
}

void Robot_UI_Layer::SyncNodeEditorModes()
{
    if (!m_NodeEditor || !m_OptionPanel || !m_OptionPanel->GetGamepadMapper())
        return;
    auto& gp = m_OptionPanel->GetGamepadMapper();
    const auto& modes = gp->GetModes();
    std::vector<std::string> names;
    for (const auto& m : modes)
        names.push_back(m.name);
    m_NodeEditor->SetModeNames(names, gp->GetActiveModeIndex());

    // Wire callback: mode switch in NodeEditor → set active mode in GamepadMapper
    // Capture raw pointer to avoid copying unique_ptr
    GamepadMapper* rawGp = gp.get();
    m_NodeEditor->SetModeSwitchCallback([rawGp](int idx) {
        rawGp->SetActiveModeByIndex(idx);
    });
}

void Robot_UI_Layer::GamepadRoutine()
{
    bool lastModeToggle = false;
    while (m_Running)
    {
        if (m_RobotAPI && m_IsConnected)
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
                                SyncNodeEditorModes();
                            }
                        }
                    }
                }
                lastModeToggle = currentModeToggle;

                // 通过节点图求值：收集所有自定义键位的值，输入节点图，输出 ActuatorData
                if (m_NodeEditor && m_OptionPanel->GetGamepadMapper()) {
                    std::map<std::string, float> keyValues;

                    // 收集当前手柄模式下所有已绑定键位名及其当前值
                    const auto& modes = m_OptionPanel->GetGamepadMapper()->GetModes();
                    int activeIdx = m_OptionPanel->GetGamepadMapper()->GetActiveModeIndex();
                    if (activeIdx >= 0 && activeIdx < (int)modes.size()) {
                        for (const auto& mapping : modes[activeIdx].mappings) {
                            if (mapping.is_bound) {
                                float v = m_OptionPanel->GetGamepadMapper()->GetKeyValue(mapping.key_name);
                                // 数字键位钳制为 0 或 1
                                if (!mapping.is_analog)
                                    v = (v >= 0.5f) ? 1.0f : 0.0f;
                                keyValues[mapping.key_name] = v;
                            }
                        }
                    }

                    // 节点图求值 → 直接写入 ActuatorData
                    m_NodeEditor->EvaluateIntoActuator(keyValues, data);
                }
            }

            {
                std::lock_guard<std::mutex> lock(m_CommandMutex);
                m_CurrentCommand = data;
            }

            if (m_IsConnected) {
                m_RobotAPI->SendActuatorData(data);
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
        if (ImGui::Begin("Option", nullptr, ImGuiWindowFlags_AlwaysVerticalScrollbar))
        {
            m_OptionPanel->DrawOptionPanel();

            float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

            if (ImGui::Button("Apply", ImVec2(buttonWidth, 0)))
            {
                if (m_OptionPanel->Apply()) {
                    m_IsConnected = false;  // 配置变更后断开连接
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
            if (m_IsConnected) {
                if (ImGui::Button("Disconnect")) {
                    m_IsConnected = false;
                }
            }
            else {
                if (ImGui::Button("Connect")) {
                    if (m_RobotAPI && m_OptionPanel->GetRobotConfig()) {
                        auto& config = m_OptionPanel->GetRobotConfig();
                        m_IsConnected = m_RobotAPI->Initialize(
                            config->GetActiveHostIP(),
                            config->GetActiveRemotePort(),
                            config->GetActiveLocalPort());
                    }
                }
            }

            ImGui::Separator();

            std::string currentModeName = "None";
            if (m_OptionPanel->GetRobotConfig()) {
                currentModeName = m_OptionPanel->GetRobotConfig()->GetActiveModeName();
            }
            ImGui::Text("Current Mode: %s", currentModeName.c_str());

            std::lock_guard<std::mutex> lock(m_CommandMutex);
            ImGui::Text("Current Motion Command:");
            ImGui::Text("X (Forward/Back): %.2f", m_CurrentCommand.motion.x);
            ImGui::Text("Y (Left/Right):   %.2f", m_CurrentCommand.motion.y);
            ImGui::Text("Z (Up/Down):      %.2f", m_CurrentCommand.motion.z);
            ImGui::Text("Roll (RX):        %.2f", m_CurrentCommand.motion.rx);
            ImGui::Text("Pitch (RY):       %.2f", m_CurrentCommand.motion.ry);
            ImGui::Text("Yaw (RZ):         %.2f", m_CurrentCommand.motion.rz);

            ImGui::Separator();
            ImGui::Text("Sensor Data:");
            if (m_RobotAPI && m_IsConnected)
            {
                SensorData sensorData = m_RobotAPI->GetSensorData();
                if (sensorData.is_valid)
                {
                    if (m_OptionPanel->GetRobotConfig()->HasTemperature())
                        ImGui::Text("Temperature: %.2f *C", sensorData.temperature.value);
                    if (m_OptionPanel->GetRobotConfig()->HasHumidity())
                        ImGui::Text("Humidity:    %.2f %%", sensorData.humidity.value);
                    if (m_OptionPanel->GetRobotConfig()->HasDepth())
                        ImGui::Text("Depth:       %.2f m", sensorData.depth.value);
                }
                else
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Sensor data offline or invalid");
                }
            }
            else
            {
                ImGui::TextDisabled("Robot API Not Connected");
            }
        }
        ImGui::End();
    }

    // 节点编辑器窗口
    if (m_NodeEditorOpen && m_NodeEditor)
    {
        // 每帧同步模式列表，确保新建/删除模式后下拉框实时更新
        SyncNodeEditorModes();

        // 同步可用键位名（自定义键位 + 映射中的键位名）
        if (m_OptionPanel && m_OptionPanel->GetGamepadMapper())
        {
            std::vector<std::string> keyNames;
            const auto& modes = m_OptionPanel->GetGamepadMapper()->GetModes();
            int activeIdx = m_OptionPanel->GetGamepadMapper()->GetActiveModeIndex();
            if (activeIdx >= 0 && activeIdx < (int)modes.size())
            {
                // 用户创建的自定义键位（在 GamepadMapper 的 Digital/Analog 区域）
                for (const auto& key : modes[activeIdx].keys)
                    keyNames.push_back(key.name);
            }
            m_NodeEditor->SetAvailableKeyNames(keyNames);
        }

        // 同步可用输出目标（来自 ActuatorConfig）
        if (m_OptionPanel && m_OptionPanel->GetRobotConfig())
        {
            ActuatorData cfg = m_OptionPanel->GetRobotConfig()->GetActiveActuatorConfig();
            auto targets = GetActuatorOutputTargets(cfg);
            m_NodeEditor->SetAvailableOutputTargets(targets);
        }

        // 每帧求值并刷新侧边栏（即使未连接手柄，也显示当前节点图的计算结果）
        if (m_OptionPanel && m_OptionPanel->GetGamepadMapper())
        {
            std::map<std::string, float> keyValues;
            const auto& modes = m_OptionPanel->GetGamepadMapper()->GetModes();
            int activeIdx = m_OptionPanel->GetGamepadMapper()->GetActiveModeIndex();
            if (activeIdx >= 0 && activeIdx < (int)modes.size())
            {
                std::set<std::string> analogKeys;
                for (const auto& mapping : modes[activeIdx].mappings)
                {
                    if (!mapping.is_bound) continue;     // 未绑定键位跳过
                    float v = m_OptionPanel->GetGamepadMapper()->GetKeyValue(mapping.key_name);
                    // 数字键位（is_analog=false）钳制为 0 或 1
                    if (!mapping.is_analog)
                        v = (v >= 0.5f) ? 1.0f : 0.0f;
                    else
                        analogKeys.insert(mapping.key_name);
                    keyValues[mapping.key_name] = v;
                }
                m_NodeEditor->SetAnalogKeys(analogKeys);

            }
            m_NodeEditor->Evaluate(keyValues);
        }

        if (!m_NodeEditor->Draw())
            m_NodeEditorOpen = false;
    }
}

// --- Entry Point ---
Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
    // 设置 GLFW 错误回调，抑制剪贴板格式不可用错误（错误码 65545 = GLFW_FORMAT_UNAVAILABLE）
    glfwSetErrorCallback([](int error, const char* description) {
        if (error == 65545) return;  // 静默剪贴板转换失败（如中文编码问题）
        fprintf(stderr, "GLFW Error %d: %s\n", error, description);
    });

    Walnut::ApplicationSpecification spec;
    spec.Name = "Robot UI";
    spec.CustomTitlebar = true;

    Walnut::Application* app = new Walnut::Application(spec);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

    std::shared_ptr<RobotAPI> hardwareAPI = std::make_shared<HardwareInterface>();

    std::shared_ptr<Robot_UI_Layer> uiLayer = std::make_shared<Robot_UI_Layer>(hardwareAPI);
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

            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Live Streamer", nullptr, &uiLayer->GetLiveStreamerOpen());
                ImGui::MenuItem("Robot Status", nullptr, &uiLayer->GetShowRobotStatus());
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Tools"))
            {
                if (ImGui::MenuItem("Option"))
                {
                    uiLayer->ShowOption();
                }
                if (ImGui::MenuItem("Node Editor"))
                {
                    uiLayer->ShowNodeEditor();
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