#include "Robot_UI.h"
#include "Robot_API/hardware_interface.h"
#include "Walnut/EntryPoint.h"
#include "ConfigSerializer.h"
#include <imgui_node_editor.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <commdlg.h>   // GetOpenFileNameA / GetSaveFileNameA

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
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = sizeof(filePath);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn))
        return filePath;
    return "";
}

static std::string Win32SaveFileDialog(const char* filter, const char* defaultExt)
{
    HWND hwnd = GetActiveWindow();
    char filePath[MAX_PATH] = "";
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = sizeof(filePath);
    ofn.lpstrDefExt = defaultExt;
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
                                streams, uiState, &error))
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
                                keyValues[mapping.key_name] = m_OptionPanel->GetGamepadMapper()->GetKeyValue(mapping.key_name);
                            }
                        }
                    }

                    // 节点图求值并直接写入 ActuatorData
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
        // 同步可用键位名（来自 GamepadMapper）
        if (m_OptionPanel && m_OptionPanel->GetGamepadMapper())
        {
            std::vector<std::string> keyNames;
            const auto& modes = m_OptionPanel->GetGamepadMapper()->GetModes();
            int activeIdx = m_OptionPanel->GetGamepadMapper()->GetActiveModeIndex();
            if (activeIdx >= 0 && activeIdx < (int)modes.size())
            {
                for (const auto& mapping : modes[activeIdx].mappings)
                    keyNames.push_back(mapping.key_name);
            }
            m_NodeEditor->SetAvailableKeyNames(keyNames);
        }

        // 同步可用输出目标（ActuatorData 字段路径）
        if (m_OptionPanel && m_OptionPanel->GetRobotConfig())
        {
            auto data = m_OptionPanel->GetRobotConfig()->GetActiveActuatorConfig();
            auto targets = GetActuatorOutputTargets(data);
            m_NodeEditor->SetAvailableOutputTargets(targets);
        }

        m_NodeEditor->Draw();
    }
}

void Robot_UI_Layer::ShowNodeEditor()
{
    m_NodeEditorOpen = true;
}

void Robot_UI_Layer::ShowThrustCurveEditor()
{
    m_ThrustCurveEditorOpen = true;
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