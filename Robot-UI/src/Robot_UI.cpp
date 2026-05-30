#include "Robot_UI.h"
#include "Robot_API/hardware_interface.h"
#include "Walnut/EntryPoint.h"
#include "Walnut/Core/Log.h"
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
    m_LiveStreamerOpen(true), m_RobotStatusOpen(true), m_NodeEditorOpen(false)
{
    WL_INFO_TAG("APP", "Robot UI initializing...");

    m_OptionPanel = std::make_unique<OptionPanel>();
    m_StreamManager = std::make_unique<StreamManager>();
    m_RobotCommManager = std::make_unique<RobotCommManager>();
    m_RobotStatus = std::make_unique<RobotStatus>();

    // 初始同步：设置 RobotStatus 的活跃模式指针
    if (m_RobotStatus && m_OptionPanel->GetRobotComponent())
    {
        auto* comp = m_OptionPanel->GetRobotComponent().get();
        m_RobotCommManager->SetRobotComponent(comp);
        m_RobotCommManager->SetGamepadMapper(m_OptionPanel->GetGamepadMapper().get());

        auto& modes = comp->GetModes();
        int idx = comp->GetActiveModeIndex();
        if (idx >= 0 && idx < (int)modes.size()) {
            m_RobotStatus->SetActiveMode(&modes[idx]);
            m_RobotStatus->LoadGraph(modes[idx].gamepad_mapping_Mode);
        }

        // Comm 面板切换 Robot Mode 时，同步 GamepadMapper + RobotStatus
        // NodeEditor 完全独立，不受 CommConfig 模式切换影响
        m_RobotCommManager->SetOnActiveModeChanged([this](int oldIdx, int newIdx) {
            if (!m_OptionPanel) return;
            auto* comp = m_OptionPanel->GetRobotComponent().get();
            if (!comp) return;
            auto& modes = comp->GetModes();
            if (newIdx < 0 || newIdx >= (int)modes.size()) return;

            // 同步 GamepadMapper → 按 RobotMode 的 gamepad_mapping_Mode 名称
            if (m_OptionPanel->GetGamepadMapper()) {
                auto& gpMapper = *m_OptionPanel->GetGamepadMapper();
                const std::string& gpModeName = modes[newIdx].gamepad_mapping_Mode;
                if (!gpModeName.empty() && gpMapper.ModeExists(gpModeName)) {
                    gpMapper.SetActiveMode(gpModeName);
                }
            }

            // 同步 RobotStatus + 加载对应节点图
            if (m_RobotStatus) {
                m_RobotStatus->SetActiveMode(&modes[newIdx]);
                m_RobotStatus->LoadGraph(modes[newIdx].gamepad_mapping_Mode);
            }
        });

        // Comm 面板切换 Gamepad Mode 时，仅同步 GamepadMapper
        // NodeEditor 完全独立，不受 CommConfig 模式切换影响
        m_RobotCommManager->SetOnGamepadModeChanged([this](int oldIdx, int newIdx) {
            if (!m_OptionPanel) return;
            auto* gpMapper = m_OptionPanel->GetGamepadMapper().get();
            if (!gpMapper) return;
            auto& gpModes = gpMapper->GetModes();
            if (newIdx < 0 || newIdx >= (int)gpModes.size()) return;

            // 不再同步 NodeEditor
        });
    }

    // NodeEditor creates its own ed::EditorContext internally
    m_NodeEditor = std::make_unique<NodeEditor>();
    m_ThrustCurveEditor = std::make_unique<ThrustCurveEditor>();

    m_Running = true;
    m_CurrentCommand.store(std::make_shared<const ActuatorConfig>(), std::memory_order_relaxed);
    m_GamepadThread = std::thread(&Robot_UI_Layer::GamepadRoutine, this);
    WL_INFO_TAG("APP", "Gamepad thread started");

    // Load default config from exe directory on startup
    std::string defaultPath = GetExeDir() + "default_config.rbt";
    WL_INFO_TAG("APP", "Loading default config: {}", defaultPath);
    LoadConfigFile(defaultPath);  // 内部已同步 RobotStatus

    WL_INFO_TAG("APP", "Robot UI initialized successfully");
}

Robot_UI_Layer::~Robot_UI_Layer()
{
    WL_INFO_TAG("APP", "Robot UI shutting down...");
    m_Running = false;
    if (m_GamepadThread.joinable())
    {
        m_GamepadThread.join();
        WL_INFO_TAG("APP", "Gamepad thread joined");
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

    LoadConfigFile(path);

    HWND hwnd = GetActiveWindow();
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

    // 保存当前节点图到 graph map（旧式 node_graph 已不再使用，统一走 graph map）
    if (m_NodeEditor) {
        m_NodeEditor->SaveGraphToMap();
    }

    std::vector<StreamConfig> streams;
    if (m_StreamManager)
        streams = m_StreamManager->GetAllStreamConfigs();

    // 收集 RobotComm 配置
    std::vector<RobotCommConfig> commConfigs;
    int commActiveId = -1;
    if (m_RobotCommManager) {
        commConfigs = m_RobotCommManager->GetAllConfigs();
        commActiveId = m_RobotCommManager->GetActiveId();
    }

    // 收集完整 UI 状态
    UIState uiState;
    uiState.about_open               = m_AboutOpen;
    uiState.option_open              = m_OptionOpen;
    uiState.live_streamer_open       = m_LiveStreamerOpen;
    uiState.robot_status_open        = m_RobotStatusOpen;
    uiState.node_editor_open         = m_NodeEditorOpen;
    uiState.thrust_curve_editor_open = m_ThrustCurveEditorOpen;
    uiState.robot_comm_open          = m_RobotCommOpen;
    if (m_OptionPanel && m_OptionPanel->GetRobotComponent())
        uiState.robot_active_mode    = m_OptionPanel->GetRobotComponent()->GetActiveModeIndex();
    if (m_OptionPanel && m_OptionPanel->GetGamepadMapper())
        uiState.gamepad_active_mode  = m_OptionPanel->GetGamepadMapper()->GetActiveModeIndex();
    if (m_NodeEditor) {
        uiState.node_left_side_width  = m_NodeEditor->GetLeftSideWidth();
        uiState.node_right_side_width = m_NodeEditor->GetRightSideWidth();
    }

    std::string error;

    if (!ConfigSerializer::Save(m_CurrentSavePath,
                                *m_OptionPanel->GetRobotComponent(),
                                *m_OptionPanel->GetGamepadMapper(),
                                *m_OptionPanel->GetImGuiStyleManager(),
                                streams, uiState,
                                &m_ThrustCurveEditor->GetCurve(),
                                commConfigs, commActiveId,
                                &m_NodeEditor->GetGraphMap(),
                                &error))
    {
        WL_ERROR_TAG("CONFIG", "Failed to save config: {} - {}", m_CurrentSavePath, error);
        MessageBoxA(GetActiveWindow(), error.c_str(), "Save Failed", MB_OK | MB_ICONWARNING);
    }
    else
    {
        WL_INFO_TAG("CONFIG", "Config saved successfully: {}", m_CurrentSavePath);
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
    std::vector<RobotCommConfig> commConfigs;
    int commActiveId = -1;
    std::map<std::string, std::string> graphMap;
    std::string error;
    if (!ConfigSerializer::Load(
            path,
            *m_OptionPanel->GetRobotComponent(),
            *m_OptionPanel->GetGamepadMapper(),
            *m_OptionPanel->GetImGuiStyleManager(),
            streams, uiState,
            &m_ThrustCurveEditor->GetCurve(),
            &commConfigs, &commActiveId,
            &graphMap,
            &error))
    {
        WL_INFO_TAG("CONFIG", "Default config not found or invalid: {} ({})", path, error);
        return;  // 静默失败（文件不存在或格式错误）
    }

    WL_INFO_TAG("CONFIG", "Default config loaded: {}", path);

    m_CurrentSavePath = path;
    if (m_RobotCommManager) m_RobotCommManager->Disconnect();

    // 恢复 StreamManager 视频流配置
    if (m_StreamManager && !streams.empty())
        m_StreamManager->LoadAllConfigs(streams);

    // 恢复 RobotCommManager 通信配置
    if (m_RobotCommManager && !commConfigs.empty())
        m_RobotCommManager->LoadConfigs(commConfigs, commActiveId);

    // 同步 RobotStatus 到新加载的活跃模式 + 加载节点图
    if (m_RobotStatus && m_OptionPanel && m_OptionPanel->GetRobotComponent())
    {
        auto* comp = m_OptionPanel->GetRobotComponent().get();
        auto& modes = comp->GetModes();
        int idx = comp->GetActiveModeIndex();
        if (idx >= 0 && idx < (int)modes.size()) {
            m_RobotStatus->SetActiveMode(&modes[idx]);
            m_RobotStatus->LoadGraph(modes[idx].gamepad_mapping_Mode);
        }
    }

    // 恢复 UI 状态
    ApplyUIState(uiState);

    // 恢复 NodeEditor 全部节点图 + 设置当前模式对
    if (m_NodeEditor && m_OptionPanel->GetRobotComponent())
    {
        auto* comp = m_OptionPanel->GetRobotComponent().get();
        auto& modes = comp->GetModes();
        int idx = comp->GetActiveModeIndex();
        std::string robotModeName = (idx >= 0 && idx < (int)modes.size()) ? std::string(modes[idx].name) : "";
        std::string gamepadModeName = (idx >= 0 && idx < (int)modes.size()) ? modes[idx].gamepad_mapping_Mode : "";

        m_NodeEditor->SetGraphMap(graphMap);
        m_NodeEditor->SetCurrentModePair(robotModeName, gamepadModeName);
    }
}

void Robot_UI_Layer::ApplyUIState(const UIState& st)
{
    m_AboutOpen               = st.about_open;
    m_OptionOpen              = st.option_open;
    m_LiveStreamerOpen        = st.live_streamer_open;
    m_RobotStatusOpen         = st.robot_status_open;
    m_NodeEditorOpen          = st.node_editor_open;
    m_ThrustCurveEditorOpen   = st.thrust_curve_editor_open;
    m_RobotCommOpen           = st.robot_comm_open;

    // 恢复 NodeEditor 侧边栏宽度
    if (m_NodeEditor) {
        m_NodeEditor->SetLeftSideWidth(st.node_left_side_width);
        m_NodeEditor->SetRightSideWidth(st.node_right_side_width);
    }
}

void Robot_UI_Layer::GamepadRoutine()
{
    WL_INFO_TAG("GAMEPAD", "Gamepad routine started (20Hz)");
    unsigned int iteration = 0;
    while (m_Running)
    {
        ++iteration;
        // Heartbeat every ~2 seconds (40 iterations at 50ms)
        if (iteration % 40 == 0)
            WL_TRACE_TAG("GAMEPAD", "Routine heartbeat #{} (connected={})", iteration, m_RobotCommManager ? m_RobotCommManager->IsConnected() : false);

        // Collect key values for sidebar display (UI thread reads via SetKeyValues)
        {
            std::map<std::string, float> keyValues;
            auto boundKeys = m_OptionPanel->GetGamepadMapper()->GetActiveModeBoundKeyNames();
            for (const auto& keyName : boundKeys) {
                keyValues[keyName] = m_OptionPanel->GetGamepadMapper()->GetKeyValue(keyName);
            }
            m_NodeEditor->SetKeyValues(keyValues);
        }

        if (m_RobotCommManager && m_RobotCommManager->IsConnected())
        {
            ActuatorConfig data;

            // 从 RobotStatus 读取已应用配置（而非直接读取 RobotComponent）
            if (m_RobotStatus) {
                data = m_RobotStatus->GetAppliedActuator();
            }

            if (m_OptionPanel->GetGamepadMapper()) {
                // 通过节点图求值：收集所有自定义键位的值，输入节点图，输出 ActuatorConfig
                // 使用 RobotStatus 的 headless graph evaluator（真正的执行图）
                if (m_RobotStatus && m_RobotStatus->HasGraphEvaluator() && m_OptionPanel->GetGamepadMapper()) {
                    std::map<std::string, float> keyValues;
                    auto boundKeys = m_OptionPanel->GetGamepadMapper()->GetActiveModeBoundKeyNames();
                    for (const auto& keyName : boundKeys) {
                        keyValues[keyName] = m_OptionPanel->GetGamepadMapper()->GetKeyValue(keyName);
                    }

                    // 节点图求值并直接写入 ActuatorConfig
                    m_RobotStatus->EvaluateIntoActuator(keyValues, data);
                }
            }

            // 无锁发布：atomic store shared_ptr，UI 线程通过 load 获取一致快照
            auto cmdPtr = std::make_shared<const ActuatorConfig>(data);
            m_CurrentCommand.store(cmdPtr, std::memory_order_release);

            // 同步到 RobotStatus（供状态窗口渲染和执行层读取）
            if (m_RobotStatus)
                m_RobotStatus->UpdateCommandData(cmdPtr);

            if (m_RobotCommManager->IsConnected()) {
                m_RobotCommManager->SendActuatorData(data);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20Hz
    }
    WL_INFO_TAG("GAMEPAD", "Gamepad routine stopped");
}

// Ensure NodeEditor's graph has valid RobotMode + GamepadMode selected.
// Called every frame from OnUIRender — only modifies state when needed.
static void NodeEditor_EnsureValidModeSelection(
    NodeEditor* editor, RobotComponent* comp, GamepadMapper* gpMapper)
{
    if (!editor) return;

    std::string curRobot = editor->GetCurrentRobotModeName();
    std::string curGp    = editor->GetCurrentGamepadModeName();

    bool robotValid = false, gpValid = false;

    // Check if current RobotMode exists in available modes
    if (comp) {
        for (const auto& mode : comp->GetActiveModes()) {
            if (mode.name == curRobot) { robotValid = true; break; }
        }
        if (!robotValid && !comp->GetActiveModes().empty()) {
            curRobot = comp->GetActiveModes()[0].name;
        }
    }

    // Check if current GamepadMode exists in available modes
    if (gpMapper) {
        for (const auto& gm : gpMapper->GetModes()) {
            if (gm.name == curGp) { gpValid = true; break; }
        }
        if (!gpValid && !gpMapper->GetModes().empty()) {
            curGp = gpMapper->GetModes()[0].name;
        }
    }

    // If either mode changed, apply to graph
    if (!robotValid || !gpValid) {
        editor->SetCurrentModePair(curRobot, curGp);
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
                m_OptionPanel->ApplyEdit();  // 内部自动同步 RobotStatus
            }

            ImGui::SameLine();

            if (ImGui::Button("Close##2", ImVec2(buttonWidth, 0)))
            {
                m_OptionOpen = false;
                m_OptionPanel->CancelEdit();
            }
        }
        ImGui::End();
    }

    if (m_RobotStatusOpen && m_RobotStatus)
    {
        m_RobotStatus->DrawWindow(&m_RobotStatusOpen, m_RobotCommManager.get());
    }

    // Node Editor window — completely independent from CommConfig
    if (m_NodeEditorOpen && m_NodeEditor)
    {
        auto* comp      = m_OptionPanel ? m_OptionPanel->GetRobotComponent().get() : nullptr;
        auto* gpMapper  = m_OptionPanel ? m_OptionPanel->GetGamepadMapper().get() : nullptr;

        // -------- Sync mode name lists (dropdown options) --------
        if (comp) {
            std::vector<std::string> robotModeNames;
            for (const auto& rm : comp->GetActiveModes())
                robotModeNames.push_back(rm.name);
            m_NodeEditor->SetRobotModeNames(robotModeNames, comp->GetActiveModeIndex());
        }
        if (gpMapper) {
            std::vector<std::string> gpModeNames;
            for (const auto& gm : gpMapper->GetModes())
                gpModeNames.push_back(gm.name);
            m_NodeEditor->SetGamepadModeNames(gpModeNames);
        }

        // Auto-init: ensure graph has a valid RobotMode + GamepadMode selected
        NodeEditor_EnsureValidModeSelection(m_NodeEditor.get(), comp, gpMapper);

        // -------- Key names (input pins) from NodeEditor's own GamepadMode --------
        if (gpMapper) {
            std::string editorGpMode = m_NodeEditor->GetCurrentGamepadModeName();
            std::vector<std::string> keyNames;
            std::set<std::string>    analogKeys;
            for (const auto& gm : gpMapper->GetModes()) {
                if (gm.name == editorGpMode) {
                    for (const auto& mapping : gm.mappings) {
                        keyNames.push_back(mapping.key_name);
                        if (mapping.is_analog)
                            analogKeys.insert(mapping.key_name);
                    }
                    break;
                }
            }
            m_NodeEditor->SetAvailableKeyNames(keyNames);
            m_NodeEditor->SetAnalogKeys(analogKeys);
        }

        // -------- Output targets from NodeEditor's own RobotMode --------
        if (comp) {
            std::string editorRobotMode = m_NodeEditor->GetCurrentRobotModeName();
            for (const auto& mode : comp->GetModes()) {
                if (mode.name == editorRobotMode) {
                    auto targets = BuildOutputTargetsFromProtocol(mode.protocol_send, mode.actuator_config);
                    m_NodeEditor->SetAvailableOutputTargets(targets);

                    std::map<std::string, double> fieldVals;
                    for (const auto& t : targets) {
                        double val = 0.0;
                        if (GetActuatorField(mode.actuator_config, t.field_path, val))
                            fieldVals[t.field_path] = val;
                    }
                    m_NodeEditor->SetFieldValues(fieldVals);
                    break;
                }
            }
        }

        m_NodeEditorOpen = m_NodeEditor->Draw();
    }

    // 推力曲线编辑器窗口
    if (m_ThrustCurveEditorOpen && m_ThrustCurveEditor)
    {
        m_ThrustCurveEditor->Draw();
        m_ThrustCurveEditorOpen = m_ThrustCurveEditor->IsOpen();
    }

    if (m_RobotCommManager)
    {
        m_RobotCommManager->DrawUI("Robot Comm", &m_RobotCommOpen);
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
        m_RobotCommManager->Open();
}

// --- Entry Point ---
Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
    Walnut::Log::Init();

    Walnut::ApplicationSpecification spec;
    spec.Name = "Robot UI";
    spec.CustomTitlebar = true;

    Walnut::Application* app = new Walnut::Application(spec);

    WL_INFO_TAG("APP", "Robot UI application created");

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