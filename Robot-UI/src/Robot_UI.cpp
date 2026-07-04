#include "Robot_UI.h"
#include "Walnut/EntryPoint.h"
#include "Screenshot.h"
#include <implot.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan.h>
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
            m_RobotStatus->SetActiveMode(items[idx]);
            auto* gpMapper = rsp->GetGamepadMapperManager().GetSelectedMapper();
            if (gpMapper)
                m_RobotStatus->LoadGraph(gpMapper->name);
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
        rsp->GetNodeGraphManager().SetShortcutManager(&m_ShortcutManager);
    }

    rsp->SetRobotStatus(m_RobotStatus.get());

    m_ThrustCurveEditor = std::make_unique<ThrustCurveEditor>();

    ImPlot::CreateContext();
    WL_INFO_TAG("APP", "ImPlot context created");

    m_Running = true;
    m_CurrentCommand.store(std::make_shared<const ActuatorConfig>(), std::memory_order_relaxed);
    m_GamepadThread = std::thread(&Robot_UI_Layer::GamepadRoutine, this);
    WL_INFO_TAG("APP", "Gamepad thread started");

    // ---- 快捷键系统初始化（在加载文件之前设置默认值） ----
    m_ShortcutManager.SetFileManager(m_FileManager.get());
    m_ShortcutManager.SetFileCallbacks(
        [this]() { FileNew(); },
        [this]() { FileOpen(); },
        [this]() { FileSave(); },
        [this]() { FileSaveAs(); });
    m_ShortcutManager.SetScreenshotCallback(
        [this]() { TakeScreenshot(); });
    m_ShortcutManager.InitPanelRefs(
        &m_OptionOpen,
        &m_RobotStatusOpen,
        &m_RobotSettingOpen,
        &m_TerminalOpen,
        &m_MonitorWallOpen,
        &m_ThrustCurveEditorOpen,
        &m_AboutOpen);
    m_OptionPanel->SetShortcutManager(&m_ShortcutManager);
    m_OptionPanel->SetMonitorWall(m_MonitorWall.get());

    LoadKernelFile(m_FileManager->DeriveKernelPath());

    std::string robotPath;
    if (m_FileManager->HasRobotPath() && std::filesystem::exists(m_FileManager->GetRobotPath()))
        robotPath = m_FileManager->GetRobotPath();
    else
        robotPath = FileManager::GetExeDir() + "..\\..\\..\\asset\\file\\default.rbt";

    WL_INFO_TAG("APP", "Loading component: {}", robotPath);
    LoadRobotFile(robotPath);

    // 如果加载的是默认 fallback 文件（非用户选择的），清空路径以触发 Save 时自动 SaveAs
    {
        std::string defaultPath = FileManager::GetExeDir() + "..\\..\\..\\asset\\file\\default.rbt";
        try {
            std::string rpCanon = std::filesystem::canonical(m_FileManager->GetRobotPath()).string();
            std::string dpCanon = std::filesystem::canonical(defaultPath).string();
            if (rpCanon == dpCanon) {
                m_FileManager->SetRobotPath("");
            }
        } catch (...) {
            if (robotPath == defaultPath && m_FileManager->GetRobotPath() == defaultPath) {
                m_FileManager->SetRobotPath("");
            }
        }
    }

    // 启动后强制断开所有连接（兜底保护）
    if (m_RobotStatus) m_RobotStatus->UnlinkAll();

    // 预热 NodeGraph 首帧（在 Splash 动画期间透明渲染一帧，避免打开时闪烁）
    m_NeedNodeGraphWarmup = false;
    {
        auto& ngMgr = rsp->GetNodeGraphManager();
        if (ngMgr.GetItemCount() > 0 && ngMgr.GetSelectedGraph())
        {
            m_NeedNodeGraphWarmup = true;
        }
    }

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

    std::vector<LiveStream> streams;
    UIState uiState;
    std::vector<RobotComm> commConfigs;
    std::map<std::string, std::string> graphMap;
    std::vector<NodeGraph> graphItems;
    std::string error;
    std::string robotShortcutsYaml;

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

    if (m_RobotStatus) m_RobotStatus->UnlinkAll();
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
            m_RobotStatus->SetActiveMode(items[idx]);
            auto* gpMapper2 = m_RobotSettingPanel->GetGamepadMapperManager().GetSelectedMapper();
            if (gpMapper2)
                m_RobotStatus->LoadGraph(gpMapper2->name);
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
        std::string robotModeName = (idx >= 0 && idx < (int)items.size()) ? std::string(items[idx].name) : "";
        std::string gamepadModeName;
        auto* gm = m_RobotSettingPanel->GetGamepadMapperManager().GetSelectedMapper();
        if (gm) gamepadModeName = gm->name;

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

    std::vector<LiveStream> streams;
    if (liveStreamMgr) streams = liveStreamMgr->GetAllItems();
    std::vector<RobotComm> commConfigs;
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
    // 恢复软件 UI 快捷键绑定
    if (!uiState.software_shortcuts_yaml.empty())
        m_ShortcutManager.LoadSoftwareBindingsFromYaml(uiState.software_shortcuts_yaml);

    // 恢复截图设置
    m_OptionPanel->SetScreenshotScope(uiState.screenshot_scope);
    m_OptionPanel->SetScreenshotPath(uiState.screenshot_path);

    // 恢复连接设置
    m_OptionPanel->SetConnRetryCount(uiState.conn_retry_count);
    m_OptionPanel->SetCameraRetryCount(uiState.camera_retry_count);
    if (m_RobotStatus) {
        m_RobotStatus->SetConnRetryCount(uiState.conn_retry_count);
        m_RobotStatus->SetCameraRetryCount(uiState.camera_retry_count);
    }

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

    // 软件 UI 快捷键绑定
    uiState.software_shortcuts_yaml = m_ShortcutManager.GetSoftwareBindingsYaml();

    // 截图设置
    uiState.screenshot_scope = m_OptionPanel->GetScreenshotScope();
    uiState.screenshot_path  = m_OptionPanel->GetScreenshotPath();

    // 连接设置
    uiState.conn_retry_count = m_OptionPanel->GetConnRetryCount();
    uiState.camera_retry_count = m_OptionPanel->GetCameraRetryCount();

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

void Robot_UI_Layer::FileNew()
{
    // 断开所有连接
    if (m_RobotStatus) m_RobotStatus->UnlinkAll();

    // 清空路径（标记为新文件）
    m_FileManager->SetRobotPath("");
    m_FileManager->MarkRobotClean();
    m_FileManager->ClearRecentFiles();

    // 重置 Component Manager：清空并创建一个默认 Component
    auto& compMgr = m_RobotSettingPanel->GetRobotComponentManager();
    auto& comps = compMgr.GetComponents();
    comps.clear();
    RobotComponent defaultComp;
    strncpy_s(defaultComp.name, "Default", sizeof(defaultComp.name) - 1);
    defaultComp.id = 0;
    comps.push_back(defaultComp);
    compMgr.SelectItem(0);

    // 重置 GamepadMapper
    auto& gpMgr = m_RobotSettingPanel->GetGamepadMapperManager();
    gpMgr.ResetToDefault();

    // 重置 NodeGraph
    m_RobotSettingPanel->GetNodeGraphManager().ResetToDefault();

    // 重置 LiveStream
    m_RobotSettingPanel->GetLiveStreamManager()->ResetToDefault();

    // 重置 RobotComm
    m_RobotSettingPanel->GetRobotCommManager()->ResetToDefault();

    // 重置 RobotStatus
    if (m_RobotStatus) {
        m_RobotStatus->SetActiveMode(comps[0]);
        auto* gpMapper = gpMgr.GetSelectedMapper();
        if (gpMapper) {
            m_RobotStatus->SetActiveGamepad(gpMapper);
            m_RobotStatus->LoadGraph(gpMapper->name);
        }
    }

    WL_INFO_TAG("APP", "New file created");
}

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

// ==================== 截图功能 ====================

void Robot_UI_Layer::TakeScreenshot()
{
    int scope = m_OptionPanel ? m_OptionPanel->GetScreenshotScope() : 0;

    std::string dir = m_OptionPanel ? m_OptionPanel->GetScreenshotPath() : "";
    if (dir.empty())
        dir = FileManager::GetExeDir() + "..\\..\\..\\asset\\screenshots";
    if (!dir.empty() && dir.back() != '\\' && dir.back() != '/')
        dir += '\\';
    std::filesystem::create_directories(dir);

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
    localtime_s(&tm_now, &t);

    const char* name = (scope >= 0 && scope < Screenshot::COUNT)
        ? Screenshot::GetWindowNames()[scope] : "";

    char filename[512];
    snprintf(filename, sizeof(filename), "%s%s_%04d%02d%02d_%02d%02d%02d.bmp",
        dir.c_str(), name,
        tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
        tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);

    if (Screenshot::Capture(scope, dir, filename))
        WL_INFO_TAG("APP", "Screenshot saved: {} ({}x{})", filename, 0, 0);
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

        // 求值始终运行（不依赖 Connect 状态），Connect 只决定是否发送
        // 但 RobotSetting 打开时由编辑器独占跑图，GamepadRoutine 跳过
        if (m_RobotStatus && m_RobotStatus->HasGraphEvaluator() && !m_RobotSettingOpen)
        {
            auto* gpMapper = m_RobotStatus->GetActiveGamepadPtr();
            if (gpMapper) {
                std::map<std::string, float> keyValues;
                auto boundKeys = gpMapper->GetActiveModeBoundKeyNames();
                for (const auto& keyName : boundKeys) {
                    keyValues[keyName] = gpMapper->GetKeyValue(keyName);
                }

                // 线程安全：快照连接池，避免与 UI 线程 SyncConnectionsFromGraph / UnlinkAll 竞态
                auto connSnap = m_RobotStatus->SnapshotConnections();
                int connCount = (int)connSnap.size();

                std::vector<ActuatorConfig> dataVec;
                {
                    auto& comps = m_RobotSettingPanel->GetRobotComponentManager().GetComponents();
                    for (int i = 0; i < connCount; ++i) {
                        int cIdx = connSnap[i].config.active_component_idx;
                        if (cIdx >= 0 && cIdx < (int)comps.size())
                            dataVec.push_back(comps[cIdx].actuator_config);
                        else
                            dataVec.push_back(m_RobotStatus->GetAppliedActuator());
                    }
                }
                std::set<int> writtenIndices;
                m_RobotStatus->EvaluateIntoActuators(keyValues, dataVec, &writtenIndices);

                // 更新 UI 显示（所有 comm 的数据）
                std::vector<std::shared_ptr<const ActuatorConfig>> cmdPtrs;
                for (int i = 0; i < (int)dataVec.size(); ++i)
                    cmdPtrs.push_back(std::make_shared<const ActuatorConfig>(dataVec[i]));
                if (!cmdPtrs.empty()) {
                    m_CurrentCommand.store(cmdPtrs[0], std::memory_order_release);
                    m_RobotStatus->UpdateAllCommandData(cmdPtrs);
                }

                // Connect 只决定是否发送，不影响解算
                for (int i = 0; i < connCount; ++i) {
                    if (!connSnap[i].isLinked) continue;
                    ActuatorConfig& data = (i < (int)dataVec.size()) ? dataVec[i] : dataVec[0];
                    connSnap[i].hw->SendActuatorData(data);
                    if (!connSnap[i].hw->IsConnected()) {
                        WL_ERROR_TAG("GAMEPAD", "Connection lost: {} ({})", connSnap[i].config.name, connSnap[i].config.host_ip);
                    }
                }
            }
        }
        int freqHz = m_RobotStatus ? m_RobotStatus->GetSendFreqHz() : 100;
        if (freqHz < 1) freqHz = 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / freqHz));
    }
    WL_INFO_TAG("GAMEPAD", "Gamepad routine stopped");
}

// ==================== 快捷键处理（委托给 ShortcutManager） ====================

// 快捷键系统已提取到 ShortcutManager.h / .cpp
// m_ShortcutManager 在构造函数中注入依赖

void Robot_UI_Layer::OnUIRender()
{
    m_ShortcutManager.Process();

    // 预热：透明渲染 NodeGraph 标签页一帧，上传 GPU 纹理，用户不可见
    if (m_NeedNodeGraphWarmup)
    {
        if (m_RobotSettingPanel)
        {
            m_RobotSettingPanel->SetWarmupMode(true);
            m_RobotSettingPanel->SelectTab(2);  // 强制切到 NodeGraph
        }
        m_RobotSettingOpen = true;
        m_RobotSettingPanel->Draw(&m_RobotSettingOpen);
        m_RobotSettingOpen = false;
        if (m_RobotSettingPanel)
        {
            m_RobotSettingPanel->SelectTab(0);      // 恢复默认 tab（Component）
            m_RobotSettingPanel->SetWarmupMode(false);
        }
        m_NeedNodeGraphWarmup = false;
    }

    // ======== Connect ↔ RobotSetting 互斥 ========
    // 规则1: 连接建立时 → 自动关闭 RobotSetting 面板
    // 规则2: 打开 RobotSetting 时 → 自动断开所有连接
    {
        static bool s_WasLinked = false;
        static bool s_WasSettingOpen = false;
        bool nowLinked = m_RobotStatus ? m_RobotStatus->IsLinked() : false;

        // 规则1: 从断链变为连接 → 关闭 RobotSetting（保存修改）
        if (!s_WasLinked && nowLinked && m_RobotSettingOpen) {
            WL_INFO_TAG("APP", "Connect detected — closing RobotSetting panel");
            m_RobotSettingOpen = false;
            if (m_RobotSettingPanel && m_RobotSettingPanel->IsEditing())
                m_RobotSettingPanel->ApplyEdit();
        }

        // 规则2: RobotSetting 从关闭变为打开且已连接 → 断开所有连接
        if (!s_WasSettingOpen && m_RobotSettingOpen && nowLinked) {
            WL_INFO_TAG("APP", "RobotSetting opened — disconnecting all");
            m_RobotStatus->UnlinkAll();
            nowLinked = false;
        }

        s_WasLinked = nowLinked;
        s_WasSettingOpen = m_RobotSettingOpen;
    }

    // 每帧将 SendAction 回调注入编辑器图，供 ShortcutTrigger 节点使用
    if (m_RobotSettingPanel && m_RobotStatus) {
        auto& mgr = m_RobotSettingPanel->GetNodeGraphManager();
        mgr.SetSendActionCb([this](int flatIdx, bool toggle, bool oneShot) {
            if (oneShot) m_RobotStatus->OneShotSendFrame(flatIdx);
            else        m_RobotStatus->ToggleSendFrame(flatIdx);
        });
        mgr.SetShortcutManager(&m_ShortcutManager);
        m_RobotStatus->SetNodeGraphManager(&mgr);
    }

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

    if (m_OptionOpen && m_OptionPanel) {
        m_OptionPanel->DrawOptionPanel(&m_OptionOpen);
        // 实时同步连接重试次数到 RobotStatus
        if (m_RobotStatus) {
            m_RobotStatus->SetConnRetryCount(m_OptionPanel->GetConnRetryCount());
            m_RobotStatus->SetCameraRetryCount(m_OptionPanel->GetCameraRetryCount());
        }
    }

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
            auto& sm = uiLayer->GetShortcutManager();
            auto hint = [&sm](int action) -> std::string { return sm.GetShortcutHint(action); };

            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New", hint(ShortcutManager::ACT_FILE_NEW).c_str()))
                {
                    uiLayer->FileNew();
                }
                ImGui::Separator();
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
                if (ImGui::MenuItem("Open File", hint(ShortcutManager::ACT_FILE_OPEN).c_str()))
                {
                    uiLayer->FileOpen();
                }
                if (ImGui::MenuItem("Save", hint(ShortcutManager::ACT_FILE_SAVE).c_str()))
                {
                    uiLayer->FileSave();
                }
                if (ImGui::MenuItem("Save As", hint(ShortcutManager::ACT_FILE_SAVEAS).c_str()))
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
                if (ImGui::MenuItem("Robot Setting", hint(ShortcutManager::ACT_TOGGLE_ROBOTSETTING).c_str()))
                {
                    uiLayer->ShowRobotSetting();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Robot Status", hint(ShortcutManager::ACT_TOGGLE_STATUS).c_str(), &uiLayer->GetShowRobotStatus());
                ImGui::MenuItem("Monitor Wall", hint(ShortcutManager::ACT_TOGGLE_MONITORWALL).c_str(), &uiLayer->GetShowMonitorWall());
                ImGui::MenuItem("Terminal", hint(ShortcutManager::ACT_TOGGLE_TERMINAL).c_str(), &uiLayer->GetShowTerminal());
                
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Tool"))
            {
                if (ImGui::MenuItem("Thrust Curve Editor", hint(ShortcutManager::ACT_TOGGLE_THRUSTCURVE).c_str()))
                {
                    uiLayer->ShowThrustCurveEditor();
                }
                if (ImGui::MenuItem("Option", hint(ShortcutManager::ACT_TOGGLE_OPTION).c_str()))
                {
                    uiLayer->ShowOption();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Screenshot", hint(ShortcutManager::ACT_SCREENSHOT).c_str()))
                {
                    uiLayer->TakeScreenshot();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help"))
            {
                if (ImGui::MenuItem("About", hint(ShortcutManager::ACT_TOGGLE_ABOUT).c_str()))
                {
                    uiLayer->ShowAbout();
                }
                ImGui::EndMenu();
            }

        });

    return app;
}

