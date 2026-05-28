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

    if (m_OptionPanel && m_OptionPanel->GetRobotComponent())
    {
        m_RobotCommManager->SetRobotComponent(m_OptionPanel->GetRobotComponent().get());

        // Comm 面板切换 Robot Mode 时，同步 RobotStatus 显示
        m_RobotCommManager->SetOnActiveModeChanged([this](int) {
            if (m_RobotStatus && m_OptionPanel->GetRobotComponent())
                m_RobotStatus->ApplyFromRobotComponent(*m_OptionPanel->GetRobotComponent());
        });
    }

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
    WL_INFO_TAG("APP", "Gamepad thread started");

    // Load default config from exe directory on startup
    std::string defaultPath = GetExeDir() + "default_config.rbt";
    WL_INFO_TAG("APP", "Loading default config: {}", defaultPath);
    LoadConfigFile(defaultPath);

    // Apply config to RobotStatus after modes are loaded
    if (m_OptionPanel && m_OptionPanel->GetRobotComponent())
    {
        m_RobotStatus->ApplyFromRobotComponent(*m_OptionPanel->GetRobotComponent());
    }

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

    std::vector<StreamConfig> streams;
    UIState uiState;
    std::string error;
    bool ok = ConfigSerializer::Load(
        path,
        *m_OptionPanel->GetRobotComponent(),
        *m_OptionPanel->GetGamepadMapper(),
        *m_OptionPanel->GetImGuiStyleManager(),
        streams,
        uiState,
        &m_ThrustCurveEditor->GetCurve(),
        &error);

    HWND hwnd = GetActiveWindow();
    if (!ok)
    {
        WL_ERROR_TAG("CONFIG", "Failed to load config: {} - {}", path, error);
        MessageBoxA(hwnd, error.c_str(), "Load Failed", MB_OK | MB_ICONWARNING);
        return;
    }

    WL_INFO_TAG("CONFIG", "Config loaded successfully: {}", path);

    m_CurrentSavePath = path;
    if (m_RobotCommManager) m_RobotCommManager->Disconnect();

    // 同步 RobotStatus 到新加载的活跃模式
    if (m_RobotStatus && m_OptionPanel->GetRobotComponent())
        m_RobotStatus->ApplyFromRobotComponent(*m_OptionPanel->GetRobotComponent());

    // 加载当前活跃模式的节点图
    if (m_NodeEditor && m_OptionPanel->GetRobotComponent())
    {
        auto& activeModes = m_OptionPanel->GetRobotComponent()->GetActiveModes();
        int idx = m_OptionPanel->GetRobotComponent()->GetActiveModeIndex();
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
    if (m_NodeEditor && m_OptionPanel->GetRobotComponent())
    {
        auto& activeModes = m_OptionPanel->GetRobotComponent()->GetActiveModes();
        int idx = m_OptionPanel->GetRobotComponent()->GetActiveModeIndex();
        if (idx >= 0 && idx < (int)activeModes.size())
            activeModes[idx].node_graph = m_NodeEditor->GetGraphYaml();
    }

    std::vector<StreamConfig> streams;
    if (m_StreamManager)
        streams = m_StreamManager->GetAllStreamConfigs();

    UIState uiState;
    std::string error;
    if (!ConfigSerializer::Save(m_CurrentSavePath,
                                *m_OptionPanel->GetRobotComponent(),
                                *m_OptionPanel->GetGamepadMapper(),
                                *m_OptionPanel->GetImGuiStyleManager(),
                                streams, uiState,
                                &m_ThrustCurveEditor->GetCurve(),
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
    std::string error;
    if (!ConfigSerializer::Load(
            path,
            *m_OptionPanel->GetRobotComponent(),
            *m_OptionPanel->GetGamepadMapper(),
            *m_OptionPanel->GetImGuiStyleManager(),
            streams, uiState,
            &m_ThrustCurveEditor->GetCurve(),
            &error))
    {
        WL_INFO_TAG("CONFIG", "Default config not found or invalid: {} ({})", path, error);
        return;  // 静默失败（文件不存在或格式错误）
    }

    WL_INFO_TAG("CONFIG", "Default config loaded: {}", path);

    m_CurrentSavePath = path;
    if (m_RobotCommManager) m_RobotCommManager->Disconnect();

    // 同步 RobotStatus 到新加载的活跃模式
    if (m_RobotStatus && m_OptionPanel && m_OptionPanel->GetRobotComponent())
        m_RobotStatus->ApplyFromRobotComponent(*m_OptionPanel->GetRobotComponent());

    // 恢复 UI 状态
    ApplyUIState(uiState);

    // 加载当前活跃模式的节点图
    if (m_NodeEditor && m_OptionPanel->GetRobotComponent())
    {
        auto& activeModes = m_OptionPanel->GetRobotComponent()->GetActiveModes();
        int idx = m_OptionPanel->GetRobotComponent()->GetActiveModeIndex();
        if (idx >= 0 && idx < (int)activeModes.size())
            m_NodeEditor->LoadGraphYaml(activeModes[idx].node_graph);
    }
}

void Robot_UI_Layer::ApplyUIState(const UIState&)
{
}

void Robot_UI_Layer::GamepadRoutine()
{
    WL_INFO_TAG("GAMEPAD", "Gamepad routine started (20Hz)");
    bool lastModeToggle = false;
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
            ActuatorData data;

            // 从 RobotStatus 读取已应用配置（而非直接读取 RobotComponent）
            if (m_RobotStatus) {
                data = m_RobotStatus->GetAppliedActuator();
            }

            if (m_OptionPanel->GetGamepadMapper()) {
                float modeToggleVal = m_OptionPanel->GetGamepadMapper()->GetKeyValue("Mode Toggle");
                bool currentModeToggle = modeToggleVal > 0.5f;
                if (currentModeToggle && !lastModeToggle) {
                    if (m_OptionPanel && m_OptionPanel->GetRobotComponent()) {
                        m_OptionPanel->GetRobotComponent()->NextActiveMode();
                        data = m_OptionPanel->GetRobotComponent()->GetActiveActuatorConfig();

                        // 同步 RobotStatus 到新模式
                        if (m_RobotStatus)
                            m_RobotStatus->ApplyFromRobotComponent(*m_OptionPanel->GetRobotComponent());

                        WL_INFO_TAG("GAMEPAD", "Mode switched -> {}",
                            m_OptionPanel->GetRobotComponent()->GetActiveModes()[
                                m_OptionPanel->GetRobotComponent()->GetActiveModeIndex()].name);

                        // 切换到新模式时，同步手柄映射预设和节点图
                        auto& active_modes = m_OptionPanel->GetRobotComponent()->GetActiveModes();
                        int active_idx = m_OptionPanel->GetRobotComponent()->GetActiveModeIndex();
                        if (!active_modes.empty() && active_idx < (int)active_modes.size()) {
                            std::string Mode = active_modes[active_idx].gamepad_mapping_Mode;
                            if (m_OptionPanel->GetGamepadMapper()->ModeExists(Mode)) {
                                m_OptionPanel->GetGamepadMapper()->SetActiveMode(Mode);
                            }
                            // Sync node graph via pending queue (must run on UI thread)
                            if (m_NodeEditor) {
                                m_NodeEditor->RequestLoadGraph(active_modes[active_idx].node_graph);
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
            auto cmdPtr = std::make_shared<const ActuatorData>(data);
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
                m_OptionPanel->ApplyEdit();
                // 同步 RobotStatus 到已应用的配置
                if (m_RobotStatus && m_OptionPanel->GetRobotComponent())
                    m_RobotStatus->ApplyFromRobotComponent(*m_OptionPanel->GetRobotComponent());
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

    // Node Editor window
    if (m_NodeEditorOpen && m_NodeEditor)
    {
        // Sync available key names (from GamepadMapper)
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
        if (m_OptionPanel && m_OptionPanel->GetRobotComponent())
        {
            auto& cfg = m_OptionPanel->GetRobotComponent();
            const auto& proto = cfg->GetActiveProtocolSendConfig();
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

void Robot_UI_Layer::SyncNodeEditorModes()
{
    if (m_NodeEditor && m_OptionPanel && m_OptionPanel->GetRobotComponent())
    {
        auto& activeModes = m_OptionPanel->GetRobotComponent()->GetActiveModes();
        int idx = m_OptionPanel->GetRobotComponent()->GetActiveModeIndex();
        if (idx >= 0 && idx < (int)activeModes.size())
            m_NodeEditor->LoadGraphYaml(activeModes[idx].node_graph);
    }
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