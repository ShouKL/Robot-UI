#include "PluginPanel.h"
#include "PluginManager.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cmath>
#include <numeric>
#include <thread>

const char* PluginPanel::StateToString(PanelState s)
{
    switch (s)
    {
    case PanelState::NotInstalled: return "Not Installed";
    case PanelState::Ready:       return "Ready";
    case PanelState::Active:      return "Active";
    case PanelState::Downloading: return "Installing";
    default: return "Unknown";
    }
}

ImVec4 PluginPanel::StateToColor(PanelState s)
{
    switch (s)
    {
    case PanelState::Active:      return ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
    case PanelState::Ready:       return ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
    case PanelState::Downloading: return ImVec4(1.0f, 0.6f, 0.2f, 1.0f);
    default: return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    }
}

void PluginPanel::ScanPluginsDir()
{
    for (auto& e : m_Entries) {
        if (e.downloading && e.dlProcess) {
            TerminateProcess((HANDLE)e.dlProcess, 1);
            CloseHandle((HANDLE)e.dlProcess);
        }
        if (e.dlPipeRead) {
            CloseHandle(e.dlPipeRead);
        }
    }
    m_Entries.clear();

    if (m_PluginsDir.empty())
        m_PluginsDir = m_PluginMgr ? m_PluginMgr->GetPluginDirectory() : "plugins";

    std::error_code ec;
    if (!std::filesystem::exists(m_PluginsDir, ec) || !std::filesystem::is_directory(m_PluginsDir, ec))
        return;

    for (auto& de : std::filesystem::directory_iterator(m_PluginsDir, ec))
    {
        if (!de.is_directory()) continue;
        std::string fn = de.path().filename().string();
        if (fn == "__pycache__" || fn == "data" || fn == "sdk" || fn[0] == '.') continue;

        auto manifest = de.path() / "plugin.json";
        if (!std::filesystem::exists(manifest)) continue;

        Entry e;
        e.folderPath = de.path().string();
        e.name = fn;
        e.hasVenv = std::filesystem::exists(de.path() / "python");

        std::ifstream mf(manifest);
        if (mf.is_open()) {
            std::stringstream buf; buf << mf.rdbuf();
            try {
                auto j = Json::parse(buf.str());
                if (j.contains("name"))    e.name        = j["name"].get<std::string>();
                if (j.contains("version")) e.version     = j["version"].get<std::string>();
                if (j.contains("author"))  e.author      = j["author"].get<std::string>();
                if (j.contains("description")) e.description = j["description"].get<std::string>();
                if (j.contains("website")) e.website     = j["website"].get<std::string>();
                if (j.contains("dependencies") && j["dependencies"].is_array())
                    for (auto& d : j["dependencies"])
                        e.dependencies.push_back(d.get<std::string>());
                if (j.contains("type") && j["type"].get<std::string>() == "native") {
                    e.isNative = true;
                    if (j.contains("main"))
                        e.nativeExe = j["main"].get<std::string>();
                    // Native plugin is "installed" if its .exe exists
                    std::string exePath = e.nativeExe.empty() ? "" : (de.path() / e.nativeExe).string();
                    e.hasVenv = !exePath.empty() && std::filesystem::exists(exePath);
                }
            } catch (...) {}
        }
        m_Entries.push_back(std::move(e));
    }
}

PluginPanel::PanelState PluginPanel::ComputeState(const Entry& e) const
{
    if (e.downloading) return PanelState::Downloading;
    if (!e.hasVenv)    return PanelState::NotInstalled;
    if (m_PluginMgr && m_PluginMgr->IsPluginEnabled(e.name)) return PanelState::Active;
    return PanelState::Ready;
}

void PluginPanel::StartDownload(int index)
{
    if (index < 0 || index >= (int)m_Entries.size()) return;
    auto& e = m_Entries[index];
    if (e.downloading) return;

    e.downloading = true;
    e.dlPhase = 0;
    e.dlStartTime = std::chrono::steady_clock::now();
    e.dlStatus = "Setting up virtual environment...";

    // 先同步删旧 .venv（C++ 重试，比 bat 里的 rd 更可控）
    RemoveVenv(e.folderPath);

    // 查找 Python：优先插件自带，其次项目内嵌
    std::string py = Plugin::FindPython(e.folderPath);

    // 写临时 .bat 避免 cmd.exe 引号嵌套问题
    std::string batPath = e.folderPath + "\\_install_deps.bat";
    {
        std::ofstream bat(batPath);
        bat << "@echo off\r\n";
        bat << "cd /d " << e.folderPath << "\r\n";
        bat << "rd /s /q .venv 2>nul\r\n";
        bat << "if exist \"%localappdata%\\pypa\\virtualenv\\Cache\" rd /s /q \"%localappdata%\\pypa\\virtualenv\\Cache\" 2>nul\r\n";
        bat << "\"" << py << "\" -m virtualenv .venv\r\n";
        bat << "if %errorlevel% neq 0 exit /b 1\r\n";
        bat << "\"" << e.folderPath << "\\.venv\\Scripts\\python.exe\" -m pip install --no-cache-dir";
        if (std::filesystem::exists(e.folderPath + "\\requirements.txt"))
            bat << " -r \"" << e.folderPath << "\\requirements.txt\"";
        else for (auto& d : e.dependencies)
            bat << " \"" << d << "\"";
        bat << "\r\n";
        bat.close();
    }

    std::string fullCmd = "cmd.exe /c \"" + batPath + "\"";

    // Create pipe for stdout capture
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hRead = nullptr, hWrite = nullptr;
    CreatePipe(&hRead, &hWrite, &sa, 0);
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 1);

    STARTUPINFOW si = { sizeof(si) };
    si.hStdOutput = hWrite;
    si.hStdError  = hWrite;
    si.dwFlags = STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi = {};
    std::wstring wcmd(fullCmd.begin(), fullCmd.end());
    if (CreateProcessW(nullptr, wcmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        CloseHandle(pi.hThread);
        CloseHandle(hWrite);
        e.dlProcess = pi.hProcess;
        e.dlPipeRead = hRead;
    }
    else
    {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        e.downloading = false;
    }
}

void PluginPanel::TickDownloads()
{
    for (auto& e : m_Entries)
    {
        if (!e.downloading) continue;

        if (e.dlPhase == 0)
        {
            // Check if process still running
            DWORD ec = STILL_ACTIVE;
            bool running = false;
            if (e.dlProcess) { GetExitCodeProcess((HANDLE)e.dlProcess, &ec); running = (ec == STILL_ACTIVE); }

            if (running)
            {
                // 读取管道输出，取最后一行作为状态文本
                DWORD avail = 0;
                if (e.dlPipeRead && PeekNamedPipe(e.dlPipeRead, nullptr, 0, nullptr, &avail, nullptr) && avail > 0)
                {
                    char buf[4096] = {};
                    DWORD read = 0;
                    if (ReadFile(e.dlPipeRead, buf, (std::min)(avail, (DWORD)sizeof(buf) - 1), &read, nullptr) && read > 0)
                    {
                        buf[read] = '\0';
                        e.dlOutput += buf;
                        // 只留最后 200 字符
                        if (e.dlOutput.size() > 200)
                            e.dlOutput = e.dlOutput.substr(e.dlOutput.size() - 200);
                        // 取最后一行非空
                        size_t lastNL = e.dlOutput.find_last_of("\r\n");
                        if (lastNL != std::string::npos)
                            e.dlOutputLines = (int)std::count(e.dlOutput.begin(), e.dlOutput.end(), '\n');
                    }
                }
                // 取最后 60 字符作为状态行
                std::string lastLine;
                if (!e.dlOutput.empty())
                {
                    size_t start = e.dlOutput.find_last_of("\r\n");
                    lastLine = (start != std::string::npos) ? e.dlOutput.substr(start + 1) : e.dlOutput;
                    // 去掉首尾空白
                    while (!lastLine.empty() && (lastLine.back() == '\r' || lastLine.back() == '\n' || lastLine.back() == ' '))
                        lastLine.pop_back();
                }
                e.dlStatus = lastLine.empty() ? "Installing..." : lastLine;
            }
            else
            {
                // Done — close pipe handles, check process exit code
                DWORD exitCode = 0;
                if (e.dlProcess)
                {
                    GetExitCodeProcess((HANDLE)e.dlProcess, &exitCode);
                    CloseHandle((HANDLE)e.dlProcess);
                    e.dlProcess = nullptr;
                }
                if (e.dlPipeRead) { CloseHandle(e.dlPipeRead); e.dlPipeRead = nullptr; }

                // 清理临时 bat 文件
                std::string bat = e.folderPath + "\\_install_deps.bat";
                std::filesystem::remove(bat);

                bool hasDir = std::filesystem::exists(e.folderPath + "\\.venv");
                bool ok = hasDir && exitCode == 0;
                e.hasVenv = ok;
                e.dlStatus = ok ? "Setup complete." : "Setup failed (exit " + std::to_string(exitCode) + ")";
                e.dlOutput.clear();
                e.dlOutputLines = 0;
                e.dlPhase = 1;
                e.dlPhaseTime = std::chrono::steady_clock::now();
            }
        }
        else
        {
            // Phase 1: hold result for 3 seconds then clear
            auto now = std::chrono::steady_clock::now();
            float hold = std::chrono::duration<float>(now - e.dlPhaseTime).count();
            if (hold >= 3.0f)
            {
                e.downloading = false;
                e.dlPhase = 0;
            }
        }
    }
}

void PluginPanel::RemoveVenv(const std::string& folderPath)
{
    std::string target = folderPath + "\\.venv";
    std::error_code ec;
    std::filesystem::remove_all(target, ec);
    // 重试，等进程退出释放文件锁
    for (int i = 0; i < 6 && std::filesystem::exists(target); i++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::filesystem::remove_all(target, ec);
    }
}

PluginPanel::PluginPanel() {}

void PluginPanel::Draw(bool* p_open)
{
    if (!ImGui::Begin("Plugin Manager", p_open)) { ImGui::End(); return; }
    if (!m_PluginMgr) { ImGui::TextColored(ImVec4(1,0.5f,0,1), "PluginManager not initialized."); ImGui::End(); return; }
    TickDownloads();
    float availW = ImGui::GetContentRegionAvail().x;
    float leftW  = availW * 0.42f;
    float rightW = availW - leftW - ImGui::GetStyle().ItemSpacing.x;
    ImGui::BeginChild("PluginList", ImVec2(leftW, 0), true);
    DrawPluginList();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("PluginDetail", ImVec2(rightW, 0), true);
    DrawPluginDetail();
    ImGui::EndChild();
    if (m_ConfirmOpen) {
        ImGui::OpenPopup(m_ConfirmTitle.c_str());
        if (ImGui::BeginPopupModal(m_ConfirmTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s", m_ConfirmMsg.c_str());
            ImGui::Spacing();
            if (ImGui::Button("Yes", ImVec2(80,0))) { if (m_ConfirmAction) m_ConfirmAction(); m_ConfirmOpen = false; ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80,0))) { m_ConfirmOpen = false; ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

void PluginPanel::DrawPluginList()
{
    float rw = 55;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - rw - 8);
    ImGui::InputTextWithHint("##search", "Filter plugins...", m_SearchBuf, sizeof(m_SearchBuf));
    ImGui::SameLine();
    if (ImGui::Button("Refresh", ImVec2(rw, 0))) ScanPluginsDir();
    ImGui::Separator();
    std::string search(m_SearchBuf);
    if (m_Entries.empty()) {
        ImGui::TextDisabled("  No plugins detected.");
        ImGui::TextDisabled("  Add plugin directories containing a");
        ImGui::TextDisabled("  plugin.json manifest to plugins/.");
        return;
    }
    for (int i = 0; i < (int)m_Entries.size(); i++) {
        auto& e = m_Entries[i];
        if (!search.empty()) {
            std::string hay = e.name + " " + e.author + " " + e.version;
            std::transform(hay.begin(), hay.end(), hay.begin(), ::tolower);
            std::string needle(search); std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
            if (hay.find(needle) == std::string::npos) continue;
        }
        PanelState ps = ComputeState(e);
        ImGui::PushID(i);
        char lbl[256]; snprintf(lbl, sizeof(lbl), "%s##%d", e.name.c_str(), i);
        if (ImGui::Selectable(lbl, m_SelectedIndex == i, ImGuiSelectableFlags_SpanAvailWidth)) m_SelectedIndex = i;
        const char* str = StateToString(ps);
        float tw = ImGui::CalcTextSize(str).x + 12;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - tw);
        ImGui::TextColored(StateToColor(ps), "%s", str);
        ImGui::TextDisabled("  %s  v%s", e.author.c_str(), e.version.c_str());
        ImGui::PopID();
    }
}

void PluginPanel::DrawPluginDetail()
{
    if (m_SelectedIndex < 0 || m_SelectedIndex >= (int)m_Entries.size()) {
        ImGui::TextDisabled("Select a plugin to inspect");
        ImGui::TextDisabled("its metadata and controls.");
        return;
    }
    auto& e = m_Entries[m_SelectedIndex];
    PanelState ps = ComputeState(e);
    float btnW = ImGui::GetContentRegionAvail().x - 10;
    ImGui::TextColored(ImVec4(1,1,1,1), "%s", e.name.c_str());
    ImGui::SameLine(); ImGui::TextColored(StateToColor(ps), "[%s]", StateToString(ps));
    if (e.isNative) { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1), "[Native C++]"); }
    ImGui::Separator(); ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1), "Version:"); ImGui::SameLine(120); ImGui::TextUnformatted(e.version.c_str());
    ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1), "Author:");  ImGui::SameLine(120); ImGui::TextUnformatted(e.author.c_str());
    if (!e.website.empty()) { ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1), "Website:"); ImGui::SameLine(120); ImGui::TextColored(ImVec4(0.4f,0.6f,1,1), "%s", e.website.c_str()); }
    ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1), "Path:");    ImGui::SameLine(120); ImGui::TextUnformatted(e.folderPath.c_str());
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1), "Description:"); ImGui::TextWrapped("%s", e.description.c_str());
    if (!e.dependencies.empty()) {
        ImGui::Spacing(); ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1), "Dependencies:");
        for (auto& d : e.dependencies) ImGui::TextDisabled("  - %s", d.c_str());
    }
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    if (ps == PanelState::Downloading) {
        bool done = (e.dlPhase == 1);
        ImVec4 statusColor = done ? ImVec4(0.3f, 0.8f, 0.3f, 1) : ImVec4(1.0f, 0.6f, 0.2f, 1);
        ImGui::TextColored(statusColor, "%s", e.dlStatus.c_str());
        if (!done && !e.dlOutput.empty())
        {
            ImGui::Spacing();
            ImGui::TextDisabled("Output:");
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.05f, 1));
            ImGui::BeginChild("dlLog", ImVec2(0, 120), true);
            ImGui::TextUnformatted(e.dlOutput.c_str());
            // 自动滚动到底部
            if (ImGui::GetScrollY() < ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
    } else {
        if (ps == PanelState::NotInstalled) {
            if (e.isNative) {
                ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Native plugin — executable not found.");
                ImGui::TextDisabled("%s", e.nativeExe.c_str());
            } else {
                if (ImGui::Button("Download", ImVec2(btnW, 32))) StartDownload(m_SelectedIndex);
            }
        }
        if (ps == PanelState::Ready) { ImGui::Spacing(); if (ImGui::Button("Activate", ImVec2(btnW, 32))) { if (!m_PluginMgr->IsPluginLoaded(e.name)) m_PluginMgr->LoadPlugin(e.folderPath); m_PluginMgr->EnablePlugin(e.name); } }
        if (ps == PanelState::Active) { ImGui::Spacing(); if (ImGui::Button("Deactivate", ImVec2(btnW, 32))) m_PluginMgr->DisablePlugin(e.name); }
        if (!e.isNative && (ps == PanelState::Ready || ps == PanelState::Active)) {
            ImGui::Spacing(); if (ImGui::Button("Uninstall", ImVec2(btnW, 28))) {
                int idx = m_SelectedIndex;
                // 先停插件，再删 .venv，否则进程锁文件删不掉
                if (m_PluginMgr->IsPluginEnabled(m_Entries[idx].name)) m_PluginMgr->DisablePlugin(m_Entries[idx].name);
                if (m_PluginMgr->IsPluginLoaded(m_Entries[idx].name))  m_PluginMgr->UnloadPlugin(m_Entries[idx].name);
                RemoveVenv(m_Entries[idx].folderPath);
                m_Entries[idx].hasVenv = false;
            }
        }
    }
}