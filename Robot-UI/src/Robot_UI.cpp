#include "Robot_UI.h"
#include "Robot_API/hardware_interface.h"
#include "Walnut/EntryPoint.h"
#include "Walnut/Core/Log.h"
#include "ConfigSerializer.h"
#include "OptionPanel.h"
#include "RobotSettingPanel.h"
#include <imgui_node_editor.h>
#include <implot.h>
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
    m_RobotStatusOpen(true), m_RobotSettingOpen(false)
{
    WL_INFO_TAG("APP", "Robot UI initializing...");

    m_OptionPanel       = std::make_unique<OptionPanel>();
    m_RobotSettingPanel = std::make_unique<RobotSettingPanel>();
    m_RobotStatus       = std::make_unique<RobotStatus>();

    auto* rsp      = m_RobotSettingPanel.get();
    auto* commMgr  = rsp->GetRobotCommManager();

    // 初始同步：设置 RobotStatus 的活跃模式指针
    if (m_RobotStatus)
    {
        auto& compMgr = rsp->GetRobotComponentManager();
        commMgr->SetRobotComponentManager(&compMgr);
        commMgr->SetGamepadMapperManager(&rsp->GetGamepadMapperManager());

        auto& items = compMgr.GetComponents();
        int idx = compMgr.GetSelectedIndex();
        if (idx >= 0 && idx < (int)items.size()) {
            m_RobotStatus->SetActiveMode(&items[idx].component);
            m_RobotStatus->LoadGraph(items[idx].component.gamepad_mapping_Mode);
        }

        // Comm 面板切换 Robot item 时，同步 GamepadMapper + RobotStatus
        commMgr->SetOnActiveModeChanged([this](int oldIdx, int newIdx) {
            if (!m_RobotSettingPanel) return;
            auto& compMgrInner = m_RobotSettingPanel->GetRobotComponentManager();
            auto& itemsInner = compMgrInner.GetComponents();
            if (newIdx < 0 || newIdx >= (int)itemsInner.size()) return;

            auto& gpModeMgr = m_RobotSettingPanel->GetGamepadMapperManager();
            const std::string& gpModeName = itemsInner[newIdx].component.gamepad_mapping_Mode;
            (void)gpModeMgr; (void)gpModeName; // FIXME: 后续根据 gpModeName 选择 Gamepad item

            if (m_RobotStatus) {
                m_RobotStatus->SetActiveMode(&itemsInner[newIdx].component);
                m_RobotStatus->LoadGraph(itemsInner[newIdx].component.gamepad_mapping_Mode);
            }
        });

        commMgr->SetOnGamepadModeChanged([this](int oldIdx, int newIdx) {
            if (!m_RobotSettingPanel) return;
            auto& gpModes = m_RobotSettingPanel->GetGamepadMapperManager().GetMappers();
            if (newIdx < 0 || newIdx >= (int)gpModes.size()) return;
            (void)oldIdx;
        });
    }

    // NodeGraphManager is owned by RobotSettingPanel, initialized in its constructor
    m_RobotSettingOpen = false;
    m_ThrustCurveEditor = std::make_unique<ThrustCurveEditor>();

    ImPlot::CreateContext();
    WL_INFO_TAG("APP", "ImPlot context created");

    m_Running = true;
    m_CurrentCommand.store(std::make_shared<const ActuatorConfig>(), std::memory_order_relaxed);
    m_GamepadThread = std::thread(&Robot_UI_Layer::GamepadRoutine, this);
    WL_INFO_TAG("APP", "Gamepad thread started");

    std::string defaultPath = GetExeDir() + "default_config.rbt";
    WL_INFO_TAG("APP", "Loading default component: {}", defaultPath);
    LoadConfigFile(defaultPath);

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
    ImPlot::DestroyContext();
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
    std::string path = Win32OpenFileDialog("Robot UI component (*.rbt)\0*.rbt\0All Files (*.*)\0*.*\0");
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

    // 保存当前节点图到 graph map
    if (m_RobotSettingPanel) {
        m_RobotSettingPanel->GetNodeGraphManager().SaveGraphToMap();
    }

    auto* liveStreamMgr = m_RobotSettingPanel->GetLiveStreamManager();
    auto* commMgr   = m_RobotSettingPanel->GetRobotCommManager();

    std::vector<StreamConfig> streams;
    if (liveStreamMgr)
        streams = liveStreamMgr->GetAllStreamConfigs();

    std::vector<RobotCommConfig> commConfigs;
    int commActiveId = -1;
    if (commMgr) {
        commConfigs = commMgr->GetAllConfigs();
        commActiveId = commMgr->GetActiveId();
    }

    UIState uiState;
    uiState.about_open               = m_AboutOpen;
    uiState.option_open              = m_OptionOpen;
    uiState.live_streamer_open       = m_RobotSettingPanel->GetLiveStreamerOpen();
    uiState.robot_status_open        = m_RobotStatusOpen;
    uiState.node_editor_open         = m_RobotSettingOpen;
    uiState.thrust_curve_editor_open = m_ThrustCurveEditorOpen;
    uiState.robot_comm_open          = m_RobotSettingPanel->GetRobotCommOpen();
    if (m_RobotSettingPanel)
        uiState.robot_active_mode    = m_RobotSettingPanel->GetRobotComponentManager().GetSelectedIndex();
    if (m_RobotSettingPanel)
        uiState.gamepad_active_mode  = m_RobotSettingPanel->GetGamepadMapperManager().GetSelectedIndex();
    if (m_RobotSettingPanel) {
        uiState.node_left_side_width  = m_RobotSettingPanel->GetNodeGraphManager().GetLeftSideWidth();
        uiState.node_right_side_width = m_RobotSettingPanel->GetNodeGraphManager().GetRightSideWidth();
    }

    std::string error;

    if (!ConfigSerializer::Save(m_CurrentSavePath,
                                m_RobotSettingPanel->GetRobotComponentManager(),
                                m_RobotSettingPanel->GetGamepadMapperManager(),
                                m_OptionPanel->GetImGuiStyleManager(),
                                streams, uiState,
                                &m_ThrustCurveEditor->GetCurve(),
                                commConfigs, commActiveId,
                                &m_RobotSettingPanel->GetNodeGraphManager().GetGraphMap(),
                                &error))
    {
        WL_ERROR_TAG("component", "Failed to save component: {} - {}", m_CurrentSavePath, error);
        MessageBoxA(GetActiveWindow(), error.c_str(), "Save Failed", MB_OK | MB_ICONWARNING);
    }
    else
    {
        WL_INFO_TAG("component", "component saved successfully: {}", m_CurrentSavePath);
    }
}

void Robot_UI_Layer::FileSaveAs()
{
    std::string path = Win32SaveFileDialog("Robot UI component (*.rbt)\0*.rbt\0All Files (*.*)\0*.*\0", "rbt");
    if (path.empty()) return;

    m_CurrentSavePath = path;
    FileSave();

    MessageBoxA(GetActiveWindow(), ("Configuration saved to:\n" + path).c_str(),
                "Save Success", MB_OK | MB_ICONINFORMATION);
}

void Robot_UI_Layer::LoadConfigFile(const std::string& path)
{
    auto* liveStreamMgr = m_RobotSettingPanel->GetLiveStreamManager();
    auto* commMgr   = m_RobotSettingPanel->GetRobotCommManager();

    std::vector<StreamConfig> streams;
    UIState uiState;
    std::vector<RobotCommConfig> commConfigs;
    int commActiveId = -1;
    std::map<std::string, std::string> graphMap;
    std::string error;
    if (!ConfigSerializer::Load(
            path,
            m_RobotSettingPanel->GetRobotComponentManager(),
            m_RobotSettingPanel->GetGamepadMapperManager(),
            m_OptionPanel->GetImGuiStyleManager(),
            streams, uiState,
            &m_ThrustCurveEditor->GetCurve(),
            &commConfigs, &commActiveId,
            &graphMap,
            &error))
    {
        WL_INFO_TAG("component", "Default component not found or invalid: {} ({})", path, error);
        return;
    }

    WL_INFO_TAG("component", "Default component loaded: {}", path);

    m_CurrentSavePath = path;
    if (commMgr) commMgr->Disconnect();

    if (liveStreamMgr && !streams.empty())
        liveStreamMgr->LoadAllConfigs(streams);

    if (commMgr && !commConfigs.empty())
        commMgr->LoadConfigs(commConfigs, commActiveId);

    if (m_RobotStatus)
    {
        auto& compMgr = m_RobotSettingPanel->GetRobotComponentManager();
        auto& items = compMgr.GetComponents();
        int idx = compMgr.GetSelectedIndex();
        if (idx >= 0 && idx < (int)items.size()) {
            m_RobotStatus->SetActiveMode(&items[idx].component);
            m_RobotStatus->LoadGraph(items[idx].component.gamepad_mapping_Mode);
        }
    }

    ApplyUIState(uiState);

    if (m_RobotSettingPanel)
    {
        auto& compMgr = m_RobotSettingPanel->GetRobotComponentManager();
        auto& items = compMgr.GetComponents();
        int idx = compMgr.GetSelectedIndex();
        std::string robotModeName = (idx >= 0 && idx < (int)items.size()) ? std::string(items[idx].component.name) : "";
        std::string gamepadModeName = (idx >= 0 && idx < (int)items.size()) ? items[idx].component.gamepad_mapping_Mode : "";

        m_RobotSettingPanel->GetNodeGraphManager().SetGraphMap(graphMap);
        m_RobotSettingPanel->GetNodeGraphManager().SetCurrentModePair(robotModeName, gamepadModeName);
    }
}

void Robot_UI_Layer::ApplyUIState(const UIState& st)
{
    m_AboutOpen               = st.about_open;
    m_OptionOpen              = st.option_open;
    m_RobotSettingPanel->GetLiveStreamerOpen()  = st.live_streamer_open;
    m_RobotStatusOpen         = st.robot_status_open;
    m_RobotSettingOpen        = st.node_editor_open;
    m_ThrustCurveEditorOpen   = st.thrust_curve_editor_open;
    m_RobotSettingPanel->GetRobotCommOpen()     = st.robot_comm_open;

    if (st.live_streamer_open || st.robot_comm_open)
        m_RobotSettingPanel->Open();

    if (m_RobotSettingPanel) {
        m_RobotSettingPanel->GetNodeGraphManager().SetLeftSideWidth(st.node_left_side_width);
        m_RobotSettingPanel->GetNodeGraphManager().SetRightSideWidth(st.node_right_side_width);
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
        auto* commMgr = m_RobotSettingPanel->GetRobotCommManager();

        if (iteration % 40 == 0)
            WL_TRACE_TAG("GAMEPAD", "Routine heartbeat #{} (connected={})", iteration, commMgr ? commMgr->IsConnected() : false);

        // Collect key values for sidebar display (UI thread reads via SetKeyValues)
        {
            std::map<std::string, float> keyValues;
            auto* gpMapper = m_RobotSettingPanel->GetGamepadMapperManager().GetSelectedMapper();
            if (gpMapper) {
                auto boundKeys = gpMapper->GetActiveModeBoundKeyNames();
                for (const auto& keyName : boundKeys) {
                    keyValues[keyName] = gpMapper->GetKeyValue(keyName);
                }
            }
            m_RobotSettingPanel->GetNodeGraphManager().SetKeyValues(keyValues);
        }

        if (commMgr && commMgr->IsConnected())
        {
            ActuatorConfig data;

            if (m_RobotStatus) {
                data = m_RobotStatus->GetAppliedActuator();
            }

            auto* gpMapper = m_RobotSettingPanel->GetGamepadMapperManager().GetSelectedMapper();
            if (gpMapper) {
                if (m_RobotStatus && m_RobotStatus->HasGraphEvaluator()) {
                    std::map<std::string, float> keyValues;
                    auto boundKeys = gpMapper->GetActiveModeBoundKeyNames();
                    for (const auto& keyName : boundKeys) {
                        keyValues[keyName] = gpMapper->GetKeyValue(keyName);
                    }

                    m_RobotStatus->EvaluateIntoActuator(keyValues, data);
                }
            }

            auto cmdPtr = std::make_shared<const ActuatorConfig>(data);
            m_CurrentCommand.store(cmdPtr, std::memory_order_release);

            if (m_RobotStatus)
                m_RobotStatus->UpdateCommandData(cmdPtr);

            if (commMgr->IsConnected()) {
                commMgr->SendActuatorData(data);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20Hz
    }
    WL_INFO_TAG("GAMEPAD", "Gamepad routine stopped");
}

void Robot_UI_Layer::OnUIRender()
{
    auto* liveStreamMgr = m_RobotSettingPanel->GetLiveStreamManager();
    auto* commMgr   = m_RobotSettingPanel->GetRobotCommManager();

    if (m_RobotSettingPanel) {
        auto* gpMapper = m_RobotSettingPanel->GetGamepadMapperManager().GetSelectedMapper();
        if (gpMapper) gpMapper->UpdateGamepadState();
    }

    if (liveStreamMgr) {
        liveStreamMgr->UpdateAll();
    }

    // Robot Setting 面板（Live Streamer + Robot Comm）
    m_RobotSettingPanel->Draw();

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
        if (ImGui::Begin("Option", &m_OptionOpen, ImGuiWindowFlags_AlwaysVerticalScrollbar))
        {
            m_OptionPanel->DrawOptionPanel();

            // 用户点击了 "Open Robot Setting" 按钮
            if (m_OptionPanel->IsRobotSettingRequested()) {
                m_OptionPanel->ClearRobotSettingRequest();
                m_RobotSettingPanel->Open();
            }

            float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

            if (ImGui::Button("Apply", ImVec2(buttonWidth, 0)))
            {
                m_OptionPanel->ApplyEdit();
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
        m_RobotStatus->DrawWindow(&m_RobotStatusOpen, commMgr);
    }

    // ---- Feed data to NodeGraphManager (graph editor lives inside RobotSettingPanel) ----
    {
        auto& ngMgr = m_RobotSettingPanel->GetNodeGraphManager();
        auto& compMgr = m_RobotSettingPanel->GetRobotComponentManager();
        auto& gpMgr   = m_RobotSettingPanel->GetGamepadMapperManager();

        // Sync item name lists (dropdown options)
        {
            std::vector<std::string> robotModeNames;
            for (const auto& c : compMgr.GetComponents())
                robotModeNames.push_back(c.component.name);
            ngMgr.SetRobotModeNames(robotModeNames, compMgr.GetSelectedIndex());
        }
        {
            std::vector<std::string> gpModeNames;
            for (const auto& gm : gpMgr.GetMappers())
                gpModeNames.push_back(gm.name);
            ngMgr.SetGamepadModeNames(gpModeNames);
        }

        // Key names (input pins) from current GamepadMapper
        {
            std::string editorGpMode = ngMgr.GetCurrentGamepadModeName();
            std::vector<std::string> keyNames;
            std::set<std::string>    analogKeys;
            for (const auto& gm : gpMgr.GetMappers()) {
                if (std::string(gm.name) == editorGpMode) {
                    for (const auto& mapping : gm.mappings) {
                        keyNames.push_back(mapping.key_name);
                        if (mapping.is_analog)
                            analogKeys.insert(mapping.key_name);
                    }
                    break;
                }
            }
            ngMgr.SetAvailableKeyNames(keyNames);
            ngMgr.SetAnalogKeys(analogKeys);
        }

        // Output targets from current RobotMode
        {
            std::string editorRobotMode = ngMgr.GetCurrentRobotModeName();
            for (const auto& c : compMgr.GetComponents()) {
                const auto& mode = c.component;
                if (std::string(mode.name) == editorRobotMode) {
                    auto targets = BuildOutputTargetsFromProtocol(mode.protocol_send, mode.actuator_config);
                    ngMgr.SetAvailableOutputTargets(targets);

                    std::map<std::string, double> fieldVals;
                    for (const auto& t : targets) {
                        double val = 0.0;
                        if (GetActuatorField(mode.actuator_config, t.field_path, val))
                            fieldVals[t.field_path] = val;
                    }
                    ngMgr.SetFieldValues(fieldVals);
                    break;
                }
            }
        }
    }

    // 推力曲线编辑器窗口
    if (m_ThrustCurveEditorOpen && m_ThrustCurveEditor)
    {
        m_ThrustCurveEditor->Draw();
        m_ThrustCurveEditorOpen = m_ThrustCurveEditor->IsOpen();
    }

}

void Robot_UI_Layer::ShowThrustCurveEditor()
{
    m_ThrustCurveEditor->Open();
    m_ThrustCurveEditorOpen = true;
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

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Robot Setting"))
                {
                    uiLayer->ShowRobotSetting();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Robot Status", nullptr, &uiLayer->GetShowRobotStatus());
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Tool"))
            {
                if (ImGui::MenuItem("ThrustCurve Editor"))
                {
                    uiLayer->ShowThrustCurveEditor();
                }
                if (ImGui::MenuItem("Option"))
                {
                    uiLayer->ShowOption();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help"))
            {
                if (ImGui::MenuItem("About"))
                {
                    uiLayer->ShowAbout();
                }
                ImGui::EndMenu();
            }

        });

    return app;
}

