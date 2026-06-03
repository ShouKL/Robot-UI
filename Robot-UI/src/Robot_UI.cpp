#include "Robot_UI.h"
#include "Robot_API/hardware_interface.h"
#include "Walnut/EntryPoint.h"
#include "Walnut/Core/Log.h"
#include "ConfigSerializer.h"
#include "FileManager.h"
#include "OptionPanel.h"
#include "RobotSettingPanel.h"
#include <imgui_node_editor.h>
#include <implot.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <filesystem>

Robot_UI_Layer::Robot_UI_Layer()
    : m_AboutOpen(false), m_OptionOpen(false),
    m_RobotSettingOpen(false), m_RobotStatusOpen(true),
    m_NotificationOpen(false), m_TerminalOpen(false)
{
    WL_INFO_TAG("APP", "Robot UI initializing...");

    m_OptionPanel        = std::make_unique<OptionPanel>();
    m_RobotSettingPanel  = std::make_unique<RobotSettingPanel>();
    m_RobotStatus        = std::make_unique<RobotStatus>();
    m_FileManager        = std::make_unique<FileManager>();
    m_NotificationPanel  = std::make_unique<NotificationPanel>();
    m_TerminalPanel      = std::make_unique<TerminalPanel>();

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
            (void)gpModeMgr; (void)gpModeName;

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
    m_ThrustCurveEditor = std::make_unique<ThrustCurveEditor>();

    ImPlot::CreateContext();
    WL_INFO_TAG("APP", "ImPlot context created");

    m_Running = true;
    m_CurrentCommand.store(std::make_shared<const ActuatorConfig>(), std::memory_order_relaxed);
    m_GamepadThread = std::thread(&Robot_UI_Layer::GamepadRoutine, this);
    WL_INFO_TAG("APP", "Gamepad thread started");

    // ---- 自动加载 .kernel（样式 + UI 状态） ----
    LoadKernelFile(m_FileManager->DeriveKernelPath());

    // ---- 加载上次的 .rbt（机器人参数），找不到则用默认 ----
    std::string robotPath;
    if (m_FileManager->HasRobotPath() && std::filesystem::exists(m_FileManager->GetRobotPath()))
    {
        robotPath = m_FileManager->GetRobotPath();
    }
    else
    {
        robotPath = FileManager::GetExeDir() + "..\\..\\..\\asset\\file\\default.rbt";
    }
    WL_INFO_TAG("APP", "Loading component: {}", robotPath);
    LoadRobotFile(robotPath);

    WL_INFO_TAG("APP", "Robot UI initialized successfully");
}

Robot_UI_Layer::~Robot_UI_Layer()
{
    WL_INFO_TAG("APP", "Robot UI shutting down...");

    // ---- 自动保存 .kernel ----
    SaveKernelFile(m_FileManager->DeriveKernelPath());

    m_Running = false;
    if (m_GamepadThread.joinable())
    {
        m_GamepadThread.join();
        WL_INFO_TAG("APP", "Gamepad thread joined");
    }
    ImPlot::DestroyContext();
}

// ==================== 文件序列化（自行处理，FileManager 仅提供工具） ====================

void Robot_UI_Layer::LoadRobotFile(const std::string& path)
{
    auto* liveStreamMgr = m_RobotSettingPanel->GetLiveStreamManager();
    auto* commMgr       = m_RobotSettingPanel->GetRobotCommManager();

    std::vector<StreamConfig> streams;
    UIState uiState;
    std::vector<RobotCommConfig> commConfigs;
    std::map<std::string, std::string> graphMap;
    std::string error;

    if (!ConfigSerializer::Load(
            path,
            m_RobotSettingPanel->GetRobotComponentManager(),
            m_RobotSettingPanel->GetGamepadMapperManager(),
            m_OptionPanel->GetImGuiStyleManager(),
            streams, uiState,
            &m_ThrustCurveEditor->GetCurve(),
            &commConfigs,
            &graphMap,
            &error))
    {
        WL_INFO_TAG("APP", "Robot config not found or invalid: {} ({})", path, error);
        // 文件无法打开则从最近列表中移除
        m_FileManager->RemoveRecentFile(path);
        return;
    }

    WL_INFO_TAG("APP", "Robot config loaded: {}", path);

    m_FileManager->SetRobotPath(path);
    m_FileManager->MarkRobotClean();
    m_FileManager->AddRecentFile(path);

    if (commMgr) commMgr->Disconnect();
    if (liveStreamMgr && !streams.empty())
        liveStreamMgr->LoadItems(streams);
    if (commMgr && !commConfigs.empty())
        commMgr->LoadItems(commConfigs);

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

    if (m_RobotSettingPanel)
    {
        auto& compMgr = m_RobotSettingPanel->GetRobotComponentManager();
        auto& items = compMgr.GetComponents();
        int idx = compMgr.GetSelectedIndex();
        std::string robotModeName = (idx >= 0 && idx < (int)items.size()) ? std::string(items[idx].component.name) : "";
        std::string gamepadModeName = (idx >= 0 && idx < (int)items.size()) ? items[idx].component.gamepad_mapping_Mode : "";

        m_RobotSettingPanel->GetNodeGraph()->SetGraphMap(graphMap);
        m_RobotSettingPanel->GetNodeGraph()->SetCurrentModePair(robotModeName, gamepadModeName);
    }
}

void Robot_UI_Layer::SaveRobotFile(const std::string& path)
{
    if (m_RobotSettingPanel)
        m_RobotSettingPanel->GetNodeGraph()->SaveGraphToMap();




    auto* liveStreamMgr = m_RobotSettingPanel->GetLiveStreamManager();
    auto* commMgr       = m_RobotSettingPanel->GetRobotCommManager();

    std::vector<StreamConfig> streams;
    if (liveStreamMgr) streams = liveStreamMgr->GetAllItems();
    std::vector<RobotCommConfig> commConfigs;
    if (commMgr) commConfigs = commMgr->GetAllItems();

    UIState uiState; // .rbt 不保存 UI 状态

    std::string error;
    if (!ConfigSerializer::Save(path,
                m_RobotSettingPanel->GetRobotComponentManager(),
                m_RobotSettingPanel->GetGamepadMapperManager(),
                m_OptionPanel->GetImGuiStyleManager(),
                streams, uiState,
                &m_ThrustCurveEditor->GetCurve(),
                commConfigs,
                &m_RobotSettingPanel->GetNodeGraph()->GetGraphMap(),
                &error))
    {
        WL_ERROR_TAG("APP", "Failed to save: {} - {}", path, error);
        MessageBoxA(GetActiveWindow(), error.c_str(), "Save Failed", MB_OK | MB_ICONWARNING);
        return;
    }

    m_FileManager->SetRobotPath(path);
    m_FileManager->MarkRobotClean();
    m_FileManager->AddRecentFile(path);
    WL_INFO_TAG("APP", "Robot config saved: {}", path);
}

void Robot_UI_Layer::LoadKernelFile(const std::string& path)
{
    UIState uiState;
    std::string error;
    if (!ConfigSerializer::LoadKernel(path, m_OptionPanel->GetImGuiStyleManager(), uiState, &error))
    {
        // 首次运行没有 .kernel 是正常的
        WL_TRACE_TAG("APP", "Kernel not loaded ({}): {}", path, error);
        return;
    }

    // 恢复 FileManager 状态
    if (!uiState.robot_path.empty())
        m_FileManager->SetRobotPath(uiState.robot_path);
    m_FileManager->SetRobotDirty(uiState.robot_dirty);
    m_FileManager->SetRecentFiles(uiState.recent_files);

    m_FileManager->MarkKernelClean();
    ApplyUIState(uiState);
    WL_INFO_TAG("APP", "Kernel loaded: {}", path);
}

void Robot_UI_Layer::SaveKernelFile(const std::string& path)
{
    UIState uiState;
    uiState.about_open               = m_AboutOpen;
    uiState.option_open              = m_OptionOpen;
    uiState.live_streamer_open       = m_RobotSettingPanel->GetLiveStreamerOpen();
    uiState.robot_status_open        = m_RobotStatusOpen;
    uiState.node_editor_open         = m_RobotSettingOpen;
    uiState.thrust_curve_editor_open = m_ThrustCurveEditorOpen;
    uiState.notification_open         = m_NotificationOpen;
    uiState.terminal_open             = m_TerminalOpen;
    uiState.robot_comm_open          = m_RobotSettingPanel->GetRobotCommOpen();
    if (m_RobotSettingPanel)
        uiState.robot_active_mode    = m_RobotSettingPanel->GetRobotComponentManager().GetSelectedIndex();
    if (m_RobotSettingPanel)
        uiState.gamepad_active_mode  = m_RobotSettingPanel->GetGamepadMapperManager().GetSelectedIndex();
    if (m_RobotSettingPanel) {
        uiState.node_left_side_width  = m_RobotSettingPanel->GetNodeGraph()->GetLeftSideWidth();
        uiState.node_right_side_width = m_RobotSettingPanel->GetNodeGraph()->GetRightSideWidth();
    }

    // FileManager 状态
    uiState.robot_path   = m_FileManager->GetRobotPath();
    uiState.robot_dirty  = m_FileManager->IsRobotDirty();
    uiState.recent_files = m_FileManager->GetRecentFiles();

    std::string error;
    if (!ConfigSerializer::SaveKernel(path, m_OptionPanel->GetImGuiStyleManager(), uiState, &error))
    {
        WL_ERROR_TAG("APP", "Failed to save kernel: {} - {}", path, error);
        return;
    }

    m_FileManager->MarkKernelClean();
    WL_INFO_TAG("APP", "Kernel saved: {}", path);
}

// ==================== 文件操作（菜单入口） ====================

void Robot_UI_Layer::FileOpen()
{
    std::string path = FileManager::OpenDialog("Robot UI component (*.rbt)\0*.rbt\0All Files (*.*)\0*.*\0");
    if (path.empty()) return;
    LoadRobotFile(path);
}

void Robot_UI_Layer::FileSave()
{
    if (!m_FileManager->HasRobotPath())
    {
        FileSaveAs();
        return;
    }
    SaveRobotFile(m_FileManager->GetRobotPath());
}

void Robot_UI_Layer::FileSaveAs()
{
    std::string path = FileManager::SaveDialog("Robot UI component (*.rbt)\0*.rbt\0All Files (*.*)\0*.*\0", "rbt");
    if (path.empty()) return;
    SaveRobotFile(path);
    MessageBoxA(GetActiveWindow(), ("Configuration saved to:\n" + path).c_str(),
                "Save Success", MB_OK | MB_ICONINFORMATION);
}

void Robot_UI_Layer::ApplyUIState(const UIState& st)
{
    m_AboutOpen               = st.about_open;
    m_OptionOpen              = st.option_open;
    m_RobotSettingPanel->GetLiveStreamerOpen()  = st.live_streamer_open;
    m_RobotStatusOpen         = st.robot_status_open;
    m_RobotSettingOpen        = st.node_editor_open;
    m_ThrustCurveEditorOpen   = st.thrust_curve_editor_open;
    m_NotificationOpen        = st.notification_open;
    m_TerminalOpen            = st.terminal_open;
    m_RobotSettingPanel->GetRobotCommOpen()     = st.robot_comm_open;

    if (m_RobotSettingPanel) {
        m_RobotSettingPanel->GetNodeGraph()->SetLeftSideWidth(st.node_left_side_width);
        m_RobotSettingPanel->GetNodeGraph()->SetRightSideWidth(st.node_right_side_width);
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
            m_RobotSettingPanel->GetNodeGraph()->SetKeyValues(keyValues);
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

    // ---- Feed data to NodeGraph BEFORE Draw so first frame has correct data ----
    {
        auto& compMgr = m_RobotSettingPanel->GetRobotComponentManager();
        auto& gpMgr   = m_RobotSettingPanel->GetGamepadMapperManager();
        auto* graph   = m_RobotSettingPanel->GetNodeGraph();

        // Sync item name lists (dropdown options)
        {
            std::vector<std::string> robotModeNames;
            for (const auto& c : compMgr.GetComponents())
                robotModeNames.push_back(c.component.name);
            graph->SetRobotModeNames(robotModeNames, compMgr.GetSelectedIndex());
        }
        {
            std::vector<std::string> gpModeNames;
            for (const auto& gm : gpMgr.GetMappers())
                gpModeNames.push_back(gm.name);
            graph->SetGamepadModeNames(gpModeNames);
        }

        // Key names (input pins) from current GamepadMapper
        {
            std::string editorGpMode = graph->GetActiveGamepadModeName();
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
            graph->SetAvailableKeyNames(keyNames);
            graph->SetAnalogKeys(analogKeys);
        }

        // Output targets from current RobotMode
        {
            std::string editorRobotMode = graph->GetActiveRobotModeName();
            for (const auto& c : compMgr.GetComponents()) {
                const auto& mode = c.component;
                if (std::string(mode.name) == editorRobotMode) {
                    auto targets = BuildOutputTargetsFromProtocol(mode.protocol_send, mode.actuator_config);
                    graph->SetAvailableOutputTargets(targets);

                    std::map<std::string, double> fieldVals;
                    for (const auto& t : targets) {
                        double val = 0.0;
                        if (GetActuatorField(mode.actuator_config, t.field_path, val))
                            fieldVals[t.field_path] = val;
                    }
                    graph->SetFieldValues(fieldVals);
                    break;
                }
            }
        }
    }

    if (m_RobotSettingOpen)
    {
        m_RobotSettingPanel->Draw(&m_RobotSettingOpen);
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
        m_OptionPanel->DrawOptionPanel(&m_OptionOpen);

    if (m_RobotStatusOpen && m_RobotStatus)
    {
        m_RobotStatus->DrawWindow(&m_RobotStatusOpen, commMgr);
    }

    // 推力曲线编辑器窗口
    if (m_ThrustCurveEditorOpen && m_ThrustCurveEditor)
    {
        m_ThrustCurveEditor->Draw();
        m_ThrustCurveEditorOpen = m_ThrustCurveEditor->IsOpen();
    }

    // Notification Window
    if (m_NotificationPanel)
        m_NotificationPanel->Draw(&m_NotificationOpen);

    // Terminal Window
    if (m_TerminalPanel)
        m_TerminalPanel->Draw(&m_TerminalOpen);

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
    NotificationPanel::InstallLogSink();   // route all WL_* logs to Output panel

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
                if (ImGui::BeginMenu("Open"))
                {
                    int idx = 0;
                    auto recentFiles = uiLayer->GetFileManager()->GetRecentFiles();
                    for (const auto& f : recentFiles)
                    {
                        size_t slash = f.find_last_of("\\/");
                        std::string label = (slash != std::string::npos) ? f.substr(slash + 1) : f;
                        ImGui::PushID(idx++);
                        if (ImGui::MenuItem(label.c_str()))
                        {
                            uiLayer->GetFileManager()->SetRobotPath(f);
                            uiLayer->GetFileManager()->MarkRobotClean();
                            uiLayer->LoadRobotFile(f);
                        }
                        ImGui::PopID();
                        if (ImGui::BeginPopupContextItem())
                        {
                            if (ImGui::MenuItem("Remove from list"))
                                uiLayer->GetFileManager()->RemoveRecentFile(f);
                            ImGui::EndPopup();
                        }
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("%s", f.c_str());
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Browse..."))
                    {
                        uiLayer->FileOpen();
                    }
                    ImGui::EndMenu();
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
                ImGui::MenuItem("Output",       nullptr, &uiLayer->GetShowNotification());
                ImGui::MenuItem("Terminal",     nullptr, &uiLayer->GetShowTerminal());
                
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Tool"))
            {
                if (ImGui::MenuItem("Thrust Curve Editor"))
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

