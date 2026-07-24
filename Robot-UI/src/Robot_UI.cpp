#include "Robot_UI.h"
#include "Walnut/EntryPoint.h"
#include "Screenshot.h"
#include <implot.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan.h>
#include <shellapi.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

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
    m_PluginMgr          = std::make_unique<PluginManager>();
    m_PluginPanel         = std::make_unique<PluginPanel>();
    m_PluginPanel->SetPluginManager(m_PluginMgr.get());
    m_PluginPanel->SetPluginsDir(m_PluginMgr->GetPluginDirectory());

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
        rsp->GetNodeGraphManager().SetRobotStatus(m_RobotStatus.get());
    }

    rsp->SetRobotStatus(m_RobotStatus.get());
    m_RobotStatus->SetGamepadMapperManager(&rsp->GetGamepadMapperManager());

    // ---- 插件系统初始化 ----
    {
        auto& compMgr = rsp->GetRobotComponentManager();
        auto& gpMgr   = rsp->GetGamepadMapperManager();
        m_PluginMgr->SetNodeGraphManager(&rsp->GetNodeGraphManager());
        m_PluginMgr->SetRobotCommManager(commMgr);
        m_PluginMgr->SetRobotComponentManager(&compMgr);
        m_PluginMgr->SetGamepadMapperManager(&gpMgr);
        m_PluginMgr->SetLiveStreamManager(rsp->GetLiveStreamManager());
        m_PluginMgr->SetRobotStatus(m_RobotStatus.get());
    }
    m_PluginMgr->ScanPlugins();
    WL_INFO_TAG("APP", "Plugin system initialized");

    m_ThrustCurveEditor = std::make_unique<ThrustCurveEditor>();

    ImPlot::CreateContext();
    WL_INFO_TAG("APP", "ImPlot context created");

    timeBeginPeriod(1);  // Windows   → 1ms

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

    // 保存 kernel 中恢复的 enabled 状态（LoadRobotFile 会覆盖 comm 配置）
    std::map<std::string, std::map<std::string, bool>> kernelSendEnabled;
    {
        auto* commMgr = m_RobotSettingPanel->GetRobotCommManager();
        if (commMgr) {
            for (auto& node : commMgr->GetNodes()) {
                for (auto& sc : node->protocol_send)
                    kernelSendEnabled[node->name][sc.name] = sc.enabled;
            }
        }
    }

    WL_INFO_TAG("APP", "Loading component: {}", robotPath);
    LoadRobotFile(robotPath);

    // 将 kernel 中保存的 enabled 状态合并回加载的 comm 配置中
    {
        auto* commMgr = m_RobotSettingPanel->GetRobotCommManager();
        if (commMgr) {
            for (auto& node : commMgr->GetNodes()) {
                auto sit = kernelSendEnabled.find(node->name);
                if (sit != kernelSendEnabled.end()) {
                    for (auto& sc : node->protocol_send) {
                        auto it = sit->second.find(sc.name);
                        if (it != sit->second.end())
                            sc.enabled = it->second;
                    }
                }
            }
        }
    }

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

    // 预热 NodeGraph 首帧
    m_NeedNodeGraphWarmup = false;
    {
        auto& ngMgr = rsp->GetNodeGraphManager();
        if (ngMgr.GetItemCount() > 0 && ngMgr.GetSelectedGraph())
            m_NeedNodeGraphWarmup = true;
    }

    // Start gamepad thread AFTER config is loaded (graph evaluator needs data)
    m_Running = true;
    m_CurrentCommand.store(std::make_shared<const ActuatorConfig>(), std::memory_order_relaxed);
    try {
        m_GamepadThread = std::thread(&Robot_UI_Layer::GamepadRoutine, this);
        WL_INFO_TAG("APP", "Gamepad thread started");
    } catch (const std::exception& e) {
        WL_ERROR_TAG("APP", "Failed to start gamepad thread: {}", e.what());
    } catch (...) {
        WL_ERROR_TAG("APP", "Failed to start gamepad thread: unknown error");
    }

    WL_INFO_TAG("APP", "Robot UI initialized successfully");
}

Robot_UI_Layer::~Robot_UI_Layer()
{
    WL_INFO_TAG("APP", "Robot UI shutting down...");

    // ---- 先断开所有硬件连接 ----
    if (m_RobotStatus)
        m_RobotStatus->UnlinkAll();

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
    // ---- Stop gamepad thread to prevent race condition with manager data reset ----
    bool wasRunning = m_GamepadThread.joinable();
    m_Running = false;
    if (m_GamepadThread.joinable())
        m_GamepadThread.join();

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
        // Restart gamepad thread only if it was running before
        if (wasRunning) {
            m_Running = true;
            try {
                m_GamepadThread = std::thread(&Robot_UI_Layer::GamepadRoutine, this);
            } catch (...) {}
        }
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

    // ---- Restart gamepad thread after all data is loaded (only if it was running) ----
    if (wasRunning) {
        m_Running = true;
        try {
            m_GamepadThread = std::thread(&Robot_UI_Layer::GamepadRoutine, this);
            WL_INFO_TAG("APP", "Gamepad thread restarted after file load");
        } catch (const std::exception& e) {
            WL_ERROR_TAG("APP", "Failed to restart gamepad thread: {}", e.what());
        } catch (...) {
            WL_ERROR_TAG("APP", "Failed to restart gamepad thread: unknown error");
        }
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

    // 恢复启用的插件列表（仅当 .kernel 中有记录时才恢复）
    if (m_PluginMgr && !uiState.enabled_plugins.empty())
        m_PluginMgr->EnablePlugins(uiState.enabled_plugins);

    // 恢复通信节点配置（发送帧 enable 状态等）
    if (!uiState.comm_configs.empty()) {
        auto* commMgr = m_RobotSettingPanel->GetRobotCommManager();
        if (commMgr)
            commMgr->LoadItems(uiState.comm_configs);
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

    // 已启用插件列表
    if (m_PluginMgr)
        uiState.enabled_plugins = m_PluginMgr->GetEnabledPluginNames();

    // 通信节点配置（含发送帧 enable 状态）
    if (auto* commMgr = m_RobotSettingPanel->GetRobotCommManager())
        uiState.comm_configs = commMgr->GetAllItems();

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
    // ---- Stop gamepad thread to prevent race condition with manager data reset ----
    bool wasRunning = m_GamepadThread.joinable();
    m_Running = false;
    if (m_GamepadThread.joinable())
        m_GamepadThread.join();

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

    // ---- Restart gamepad thread after all data is reset (only if it was running) ----
    if (wasRunning) {
        m_Running = true;
        try {
            m_GamepadThread = std::thread(&Robot_UI_Layer::GamepadRoutine, this);
        } catch (...) {}
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
    WL_INFO_TAG("GAMEPAD", "Gamepad routine started");
    unsigned int iteration = 0;
    while (m_Running)
    {
        ++iteration;
        try
        {
            auto t0 = std::chrono::steady_clock::now();

            // Collect key values + evaluate + push to Status display (ALWAYS)
            {
                std::map<std::string, float> keyValues;
                if (m_RobotStatus) {
                    auto* gpMapper = m_RobotStatus->GetActiveGamepadPtr();
                    if (gpMapper) {
                        auto boundKeys = gpMapper->GetActiveModeBoundKeyNames();
                        for (const auto& keyName : boundKeys)
                            keyValues[keyName] = gpMapper->GetKeyValue(keyName);
                    }
                }
                if (m_RobotSettingPanel)
                    m_RobotSettingPanel->GetNodeGraph()->SetKeyValues(keyValues);

                // Build data vector: conn-based or fallback to selected component
                std::vector<ActuatorConfig> dataVec;
                if (m_RobotStatus && m_RobotSettingPanel) {
                    auto connSnap = m_RobotStatus->SnapshotConnections();
                    int connCount = (int)connSnap.size();
                    auto& comps = m_RobotSettingPanel->GetRobotComponentManager().GetComponents();
                    int selIdx = m_RobotSettingPanel->GetRobotComponentManager().GetSelectedIndex();
                    for (int i = 0; i < connCount; ++i) {
                        int cIdx = connSnap[i].config.active_component_idx;
                        if (cIdx >= 0 && cIdx < (int)comps.size())
                            dataVec.push_back(comps[cIdx].actuator_config);
                        else if (selIdx >= 0 && selIdx < (int)comps.size())
                            dataVec.push_back(comps[selIdx].actuator_config);
                    }
                    // Fallback: no connections → use selected component's real config
                    if (dataVec.empty() && selIdx >= 0 && selIdx < (int)comps.size())
                        dataVec.push_back(comps[selIdx].actuator_config);
                    if (dataVec.empty())
                        dataVec.push_back(ActuatorConfig{});

                    // Evaluate graph into dataVec
                    std::set<int> writtenIndices;
                    m_RobotStatus->EvaluateIntoActuators(keyValues, dataVec, &writtenIndices);

                    // Push result to Status display
                    std::vector<std::shared_ptr<const ActuatorConfig>> cmdPtrs;
                    for (int i = 0; i < (int)dataVec.size(); ++i)
                        cmdPtrs.push_back(std::make_shared<const ActuatorConfig>(dataVec[i]));
                    if (!cmdPtrs.empty()) {
                        m_CurrentCommand.store(cmdPtrs[0], std::memory_order_release);
                        m_RobotStatus->UpdateAllCommandData(cmdPtrs);
                    }

                    // Send only when linked
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
            auto target = t0 + std::chrono::microseconds(1000000 / freqHz);
            auto now = std::chrono::steady_clock::now();
            if (now < target) {
                auto remain = target - now;
                if (freqHz >= 200)
                    while (std::chrono::steady_clock::now() < target) YieldProcessor();
                else
                    std::this_thread::sleep_for(remain);
            }
            // 如果本帧超时，不补 sleep，直接进入下一帧
        } // try
        catch (const std::exception& e)
        {
            WL_ERROR_TAG("GAMEPAD", "Exception in loop #{}: {}", iteration, e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        catch (...)
        {
            WL_ERROR_TAG("GAMEPAD", "Unknown exception in loop #{}", iteration);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
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
            if (!m_RobotStatus) return;
            if (oneShot) m_RobotStatus->OneShotSendFrame(flatIdx);
            else        m_RobotStatus->ToggleSendFrame(flatIdx);
        });
        mgr.SetShortcutManager(&m_ShortcutManager);
        m_RobotStatus->SetEvaluatorShortcutManager(&m_ShortcutManager);
        m_RobotStatus->SetEvaluatorSendActionCb([this](int flatIdx, bool toggle, bool oneShot) {
            if (oneShot) m_RobotStatus->OneShotSendFrame(flatIdx);
            else        m_RobotStatus->ToggleSendFrame(flatIdx);
        });
        m_RobotStatus->SetNodeGraphManager(&mgr);
    }

    auto* liveStreamMgr = m_RobotSettingPanel->GetLiveStreamManager();

    if (m_RobotSettingPanel) {
        auto* gpMapper = m_RobotStatus ? m_RobotStatus->GetActiveGamepadPtr() : nullptr;
        if (gpMapper) gpMapper->UpdateGamepadState();
        if (auto* selMapper = m_RobotSettingPanel->GetGamepadMapperManager().GetSelectedMapper())
            selMapper->UpdateGamepadState();
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

    // ---- 插件系统渲染 ----
    if (m_PluginMgr)
    {
        m_PluginMgr->OnUpdate(ImGui::GetIO().DeltaTime);
        m_PluginMgr->OnUIRender();
    }

    // Plugin Manager 面板
    if (m_PluginManagerOpen && m_PluginPanel)
    {
        m_PluginPanel->Draw(&m_PluginManagerOpen);
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
                if (ImGui::MenuItem("New", "Ctrl+N"))
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
                if (ImGui::MenuItem("Open File", "Ctrl+O"))
                {
                    uiLayer->FileOpen();
                }
                if (ImGui::MenuItem("Save", "Ctrl+S"))
                {
                    uiLayer->FileSave();
                }
                if (ImGui::MenuItem("Save As", "Ctrl+Shift+S"))
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
                if (ImGui::MenuItem("Robot Setting", "F3"))
                {
                    uiLayer->ShowRobotSetting();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Robot Status", "F2", &uiLayer->GetShowRobotStatus());
                ImGui::MenuItem("Monitor Wall", "F5", &uiLayer->GetShowMonitorWall());
                ImGui::MenuItem("Terminal", "F4", &uiLayer->GetShowTerminal());
                ImGui::MenuItem("Plugin Manager", nullptr, &uiLayer->GetShowPluginManager());

                ImGui::EndMenu();
            }

            // ---- Plugins：由 PluginManager 渲染插件注册的菜单项 ----
            {
                auto* pm = uiLayer->GetPluginManager();
                if (pm)
                    pm->OnMenuBar();
            }

            if (ImGui::BeginMenu("Tool"))
            {
                if (ImGui::MenuItem("Thrust Curve Editor", "F6"))
                {
                    uiLayer->ShowThrustCurveEditor();
                }
                if (ImGui::MenuItem("Option", "F1"))
                {
                    uiLayer->ShowOption();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Screenshot", "F12"))
                {
                    uiLayer->TakeScreenshot();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help"))
            {
                if (ImGui::MenuItem("About", "F7"))
                {
                    uiLayer->ShowAbout();
                }
                ImGui::EndMenu();
            }

        });

    return app;
}

