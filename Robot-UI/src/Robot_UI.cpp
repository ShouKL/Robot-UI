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
#include <vulkan/vulkan.h>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <shellapi.h>

Robot_UI_Layer::Robot_UI_Layer()
    : m_AboutOpen(false), m_OptionOpen(false),
    m_RobotSettingOpen(false), m_RobotStatusOpen(true),
    m_TerminalOpen(false)
{
    WL_INFO_TAG("APP", "Robot UI initializing...");

    m_OptionPanel        = std::make_unique<OptionPanel>();
    m_RobotSettingPanel  = std::make_unique<RobotSettingPanel>();
    m_RobotStatus        = std::make_unique<RobotStatus>();
    m_MonitorWall        = std::make_unique<MonitorWall>();
    m_FileManager        = std::make_unique<FileManager>();
    m_TerminalPanel      = std::make_unique<TerminalPanel>();

    auto* rsp      = m_RobotSettingPanel.get();
    auto* commMgr  = rsp->GetRobotCommManager();
    m_MonitorWall->SetLiveStreamManager(rsp->GetLiveStreamManager());

    if (m_RobotStatus)
    {
        auto& compMgr = rsp->GetRobotComponentManager();
        commMgr->SetRobotComponentManager(&compMgr);

        auto& items = compMgr.GetComponents();
        int idx = compMgr.GetSelectedIndex();
        if (idx >= 0 && idx < (int)items.size()) {
            m_RobotStatus->SetActiveMode(&items[idx].component);
            m_RobotStatus->LoadGraph(items[idx].component.gamepad_mapping_Mode);
        }

        auto* gpMapper = rsp->GetGamepadMapperManager().GetSelectedMapper();
        if (gpMapper)
            m_RobotStatus->SetActiveGamepad(gpMapper);
    }

    {
        auto& compMgr = rsp->GetRobotComponentManager();
        auto& gpMgr   = rsp->GetGamepadMapperManager();
        rsp->GetNodeGraphManager().SetRobotComponentManager(&compMgr);
        rsp->GetNodeGraphManager().SetGamepadMapperManager(&gpMgr);
        rsp->GetNodeGraphManager().SetRobotCommManager(commMgr);
    }

    rsp->SetRobotStatus(m_RobotStatus.get());

    m_ThrustCurveEditor = std::make_unique<ThrustCurveEditor>();

    ImPlot::CreateContext();
    WL_INFO_TAG("APP", "ImPlot context created");

    m_Running = true;
    m_CurrentCommand.store(std::make_shared<const ActuatorConfig>(), std::memory_order_relaxed);
    m_GamepadThread = std::thread(&Robot_UI_Layer::GamepadRoutine, this);
    WL_INFO_TAG("APP", "Gamepad thread started");

    LoadKernelFile(m_FileManager->DeriveKernelPath());

    std::string robotPath;
    if (m_FileManager->HasRobotPath() && std::filesystem::exists(m_FileManager->GetRobotPath()))
        robotPath = m_FileManager->GetRobotPath();
    else
        robotPath = FileManager::GetExeDir() + "..\\..\\..\\asset\\file\\default.rbt";

    WL_INFO_TAG("APP", "Loading component: {}", robotPath);
    LoadRobotFile(robotPath);

    WL_INFO_TAG("APP", "Robot UI initialized successfully");
}

Robot_UI_Layer::~Robot_UI_Layer()
{
    WL_INFO_TAG("APP", "Robot UI shutting down...");

    // ---- 自动保存 .kernel ----
    if (m_FileManager)
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

    std::vector<StreamConfig> streams;
    UIState uiState;
    std::vector<RobotCommConfig> commConfigs;
    std::map<std::string, std::string> graphMap;
    std::vector<GraphItem> graphItems;
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
            &graphItems,
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

    if (m_RobotStatus) m_RobotStatus->Unlink();
    if (liveStreamMgr && !streams.empty())
        liveStreamMgr->LoadItems(streams);
    auto* commMgr = m_RobotSettingPanel->GetRobotCommManager();
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
        auto* gpMapper = m_RobotSettingPanel->GetGamepadMapperManager().GetSelectedMapper();
        if (gpMapper)
            m_RobotStatus->SetActiveGamepad(gpMapper);
    }

    // 恢复 NodeGraph 的编辑项列表（必须在 SetCurrentModePair 之前加载）
    if (m_RobotSettingPanel && !graphItems.empty())
        m_RobotSettingPanel->GetNodeGraphManager().LoadItems(graphItems);

    // SetGraphMap / SetCurrentModePair 仅在首次无 graphItems 时作为兜底
    if (m_RobotSettingPanel && graphItems.empty())
    {
        auto& compMgr = m_RobotSettingPanel->GetRobotComponentManager();
        auto& items = compMgr.GetComponents();
        int idx = compMgr.GetSelectedIndex();
        std::string robotModeName = (idx >= 0 && idx < (int)items.size()) ? std::string(items[idx].component.name) : "";
        std::string gamepadModeName = (idx >= 0 && idx < (int)items.size()) ? items[idx].component.gamepad_mapping_Mode : "";

        m_RobotSettingPanel->GetNodeGraph()->SetGraphMap(graphMap);
        m_RobotSettingPanel->GetNodeGraph()->SetCurrentModePair(robotModeName, gamepadModeName);
    }
    else if (m_RobotSettingPanel)
    {
        m_RobotSettingPanel->GetNodeGraph()->SetGraphMap(graphMap);
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

    auto graphItems = m_RobotSettingPanel->GetNodeGraphManager().GetAllItems();

    std::string error;
    if (!ConfigSerializer::Save(path,
                m_RobotSettingPanel->GetRobotComponentManager(),
                m_RobotSettingPanel->GetGamepadMapperManager(),
                m_OptionPanel->GetImGuiStyleManager(),
                streams, uiState,
                &m_ThrustCurveEditor->GetCurve(),
                commConfigs,
                &m_RobotSettingPanel->GetNodeGraph()->GetGraphMap(),
                &graphItems,
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

    // 恢复 FileManager 状态 — 将相对路径还原为绝对路径
    if (!uiState.robot_path.empty())
        m_FileManager->SetRobotPath(FileManager::ToAbsolutePath(uiState.robot_path));
    m_FileManager->SetRobotDirty(uiState.robot_dirty);
    {
        std::vector<std::string> absRecent;
        for (const auto& f : uiState.recent_files)
            absRecent.push_back(FileManager::ToAbsolutePath(f));
        m_FileManager->SetRecentFiles(absRecent);
    }

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
    uiState.notification_open         = m_TerminalOpen;
    uiState.terminal_open             = m_TerminalOpen;
    uiState.monitor_wall_open         = m_MonitorWallOpen;
    uiState.robot_comm_open          = m_RobotSettingPanel->GetRobotCommOpen();
    if (m_RobotSettingPanel)
        uiState.robot_active_mode    = m_RobotSettingPanel->GetRobotComponentManager().GetSelectedIndex();
    if (m_RobotSettingPanel)
        uiState.gamepad_active_mode  = m_RobotSettingPanel->GetGamepadMapperManager().GetSelectedIndex();
    if (m_RobotSettingPanel) {
        uiState.node_left_side_width  = m_RobotSettingPanel->GetNodeGraph()->GetLeftSideWidth();
        uiState.node_right_side_width = m_RobotSettingPanel->GetNodeGraph()->GetRightSideWidth();
    }

    // FileManager 状态 — 保存为相对路径（相对于 exe 目录），保证跨电脑可移植
    uiState.robot_path   = FileManager::ToRelativePath(m_FileManager->GetRobotPath());
    uiState.robot_dirty  = m_FileManager->IsRobotDirty();
    uiState.recent_files.clear();
    for (const auto& f : m_FileManager->GetRecentFiles())
        uiState.recent_files.push_back(FileManager::ToRelativePath(f));

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
    m_TerminalOpen            = st.notification_open || st.terminal_open;
    m_MonitorWallOpen         = st.monitor_wall_open;
    m_RobotSettingPanel->GetRobotCommOpen()     = st.robot_comm_open;

    if (m_RobotSettingPanel) {
        m_RobotSettingPanel->GetNodeGraph()->SetLeftSideWidth(st.node_left_side_width);
        m_RobotSettingPanel->GetNodeGraph()->SetRightSideWidth(st.node_right_side_width);
    }
}

void Robot_UI_Layer::GamepadRoutine()
{
    WL_INFO_TAG("GAMEPAD", "Gamepad routine started (100Hz)");
    unsigned int iteration = 0;
    while (m_Running)
    {
        ++iteration;
        // Heartbeat every ~1 second (100 iterations at 10ms)

        if (iteration % 100 == 0)
            WL_TRACE_TAG("GAMEPAD", "Routine heartbeat #{} (linked={})", iteration, m_RobotStatus ? m_RobotStatus->IsLinked() : false);

        // Collect key values for sidebar display (UI thread reads via SetKeyValues)
        {
            std::map<std::string, float> keyValues;
            auto* gpMapper = m_RobotStatus ? m_RobotStatus->GetActiveGamepadPtr() : nullptr;
            if (gpMapper) {
                auto boundKeys = gpMapper->GetActiveModeBoundKeyNames();
                for (const auto& keyName : boundKeys) {
                    keyValues[keyName] = gpMapper->GetKeyValue(keyName);
                }
            }
            m_RobotSettingPanel->GetNodeGraph()->SetKeyValues(keyValues);
        }

        if (m_RobotStatus && m_RobotStatus->IsLinked())
        {
            ActuatorConfig data;

            if (m_RobotStatus) {
                data = m_RobotStatus->GetAppliedActuator();
            }

            auto* gpMapper = m_RobotStatus ? m_RobotStatus->GetActiveGamepadPtr() : nullptr;
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

            m_RobotStatus->SendActuatorData(data);
        }
        int freqHz = m_RobotStatus ? m_RobotStatus->GetSendFreqHz() : 100;
        if (freqHz < 1) freqHz = 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / freqHz));
    }
    WL_INFO_TAG("GAMEPAD", "Gamepad routine stopped");
}

void Robot_UI_Layer::OnUIRender()
{
    auto* liveStreamMgr = m_RobotSettingPanel->GetLiveStreamManager();

    if (m_RobotSettingPanel) {
        auto* gpMapper = m_RobotStatus ? m_RobotStatus->GetActiveGamepadPtr() : nullptr;
        if (gpMapper) gpMapper->UpdateGamepadState();
    }

    if (m_RobotSettingOpen)
    {
        m_RobotSettingPanel->Draw(&m_RobotSettingOpen);
    }

    if (m_AboutOpen)
    {
        ImGui::SetNextWindowSize(ImVec2(420, 420), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("About", &m_AboutOpen,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImVec2 windowSize = ImGui::GetWindowSize();
            float buttonHeight = 30.0f;
            float padding = 10.0f;

            // ---- 内容区域（可滚动子区域）----
            ImGui::BeginChild("AboutContent", ImVec2(0, windowSize.y - buttonHeight - padding * 3), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            {
            // ---- 标题 & 版本 ----
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
            ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Robot UI");
            ImGui::PopFont();
            ImGui::SameLine();
            ImGui::TextDisabled("v1.0.0");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // ---- 构建信息 ----
            if (ImGui::CollapsingHeader("Build", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("Version:    v1.0.0");
#if defined(WL_DEBUG)
                ImGui::Text("Config:     Debug");
#elif defined(WL_RELEASE)
                ImGui::Text("Config:     Release");
#elif defined(WL_DIST)
                ImGui::Text("Config:     Distribution");
#else
                ImGui::Text("Config:     Unknown");
#endif
                ImGui::Text("Built:      %s %s", __DATE__, __TIME__);
#ifdef _MSC_VER
                ImGui::Text("Compiler:   MSVC %d.%02d", _MSC_VER / 100, _MSC_VER % 100);
#elif defined(__GNUC__)
                ImGui::Text("Compiler:   GCC %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(__clang__)
                ImGui::Text("Compiler:   Clang %d.%d.%d", __clang_major__, __clang_minor__, __clang_patchlevel__);
#endif
                ImGui::Text("Platform:   Windows x64");
            }

            ImGui::Spacing();

            // ---- 图形后端 ----
            if (ImGui::CollapsingHeader("Graphics"))
            {
                ImGui::Text("Backend:    Vulkan");
                VkPhysicalDeviceProperties props = {};
                vkGetPhysicalDeviceProperties(Walnut::Application::GetPhysicalDevice(), &props);
                ImGui::Text("GPU:        %s", props.deviceName);
                ImGui::Text("Vulkan:     %d.%d.%d",
                    VK_API_VERSION_MAJOR(props.apiVersion),
                    VK_API_VERSION_MINOR(props.apiVersion),
                    VK_API_VERSION_PATCH(props.apiVersion));
                ImGui::Text("Driver:     %d.%d.%d",
                    VK_API_VERSION_MAJOR(props.driverVersion),
                    VK_API_VERSION_MINOR(props.driverVersion),
                    VK_API_VERSION_PATCH(props.driverVersion));
            }

            ImGui::Spacing();

            // ---- 依赖与致谢 ----
            if (ImGui::CollapsingHeader("Third-Party"))
            {
                ImGui::BulletText("Dear ImGui (docking branch)");
                ImGui::BulletText("ImPlot v0.16");
                ImGui::BulletText("imgui-node-editor");
                ImGui::BulletText("GLFW 3.x");
                ImGui::BulletText("Vulkan SDK");
                ImGui::BulletText("yaml-cpp");
                ImGui::BulletText("spdlog");
                ImGui::BulletText("glm");
                ImGui::BulletText("stb_image");
                ImGui::BulletText("Walnut (application framework)");
                ImGui::BulletText("ImTerm (terminal emulator)");
            }

            ImGui::Spacing();

            // ---- 资源链接 ----
            if (ImGui::CollapsingHeader("Resources"))
            {
                ImGui::Text("Wiki: ");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "github.com/ShouKL/Robot-UI/wiki");
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Click to open in browser");
                    if (ImGui::IsMouseClicked(0))
                        ShellExecuteA(nullptr, "open", "https://github.com/ShouKL/Robot-UI/wiki/README.md", nullptr, nullptr, SW_SHOWNORMAL);
                }
            }

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::TextDisabled("(c) 2025-2026  Robot UI Team");
            }
            ImGui::EndChild();

            // ---- 底部固定 Close 按钮 ----
            ImGui::SetCursorPosY(windowSize.y - buttonHeight - padding);
            if (ImGui::Button("Close", ImVec2(-1, buttonHeight)))
                m_AboutOpen = false;
        }
        ImGui::End();
    }

    if (m_OptionOpen && m_OptionPanel)
        m_OptionPanel->DrawOptionPanel(&m_OptionOpen);

    if (m_RobotStatusOpen && m_RobotStatus)
    {
        m_RobotStatus->DrawWindow(&m_RobotStatusOpen,
            m_RobotSettingPanel->GetRobotCommManager(),
            m_RobotSettingPanel->GetLiveStreamManager(),
            &m_RobotSettingPanel->GetNodeGraphManager(),
            &m_RobotSettingPanel->GetRobotComponentManager(),
            &m_RobotSettingPanel->GetGamepadMapperManager());
    }

    // MonitorWall — 独立窗口，不随 Connect/Disconnect 自动开关
    if (m_MonitorWallOpen && m_MonitorWall)
    {
        m_MonitorWall->SetActiveStreamIndex(m_RobotStatus->GetActiveLiveStreamIdx());

        // 检测 RobotStatus 连接状态变化，同步流启停
        bool linked = m_RobotStatus->IsLinked();
        if (linked != m_LastLinkState) {
            m_LastLinkState = linked;
            if (linked)
                m_MonitorWall->ConnectStream();
            else
                m_MonitorWall->DisconnectStream();
        }

        m_MonitorWall->Draw(&m_MonitorWallOpen);
    }

    // 推力曲线编辑器窗口
    if (m_ThrustCurveEditorOpen && m_ThrustCurveEditor)
    {
        m_ThrustCurveEditor->Draw();
        m_ThrustCurveEditorOpen = m_ThrustCurveEditor->IsOpen();
    }

    // Terminal / Output (merged into one panel)
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

    Walnut::ApplicationSpecification spec;
    spec.Name = "Robot UI";
    spec.CustomTitlebar = true;
    spec.CenterWindow = true;

    spec.IconPath = FileManager::GetExeDir() + "..\\..\\..\\asset\\picture\\Kernel.png";

    Walnut::Application* app = new Walnut::Application(spec);

    WL_INFO_TAG("APP", "Robot UI application created");

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
                ImGui::MenuItem("Monitor Wall", nullptr, &uiLayer->GetShowMonitorWall());
                ImGui::MenuItem("Terminal", nullptr, &uiLayer->GetShowTerminal());
                
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

