#include "TerminalPanel.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

TerminalPanel::TerminalPanel()
{
    m_WorkingDir = std::filesystem::current_path().string();
    char buf[512];
    snprintf(buf, sizeof(buf), "Terminal ready -- %s", m_WorkingDir.c_str());
    Append(buf);
    Append("Type 'help' for available commands, 'cls' to clear.");
    Append("---");
    memset(m_InputBuf, 0, sizeof(m_InputBuf));
}

void TerminalPanel::Append(const std::string& text, bool isInput)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Lines.push_back({text, isInput});
    while (m_Lines.size() > MaxLines)
        m_Lines.pop_front();
}

void TerminalPanel::Clear()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Lines.clear();
}

void TerminalPanel::ExecuteCommand(const std::string& cmd)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "%s> %s", m_WorkingDir.c_str(), cmd.c_str());
    Append(buf, true);

    std::string trimmed = cmd;
    while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t'))
        trimmed.pop_back();
    while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t'))
        trimmed.erase(0, 1);

    if (trimmed.empty()) return;

    if (trimmed == "cls" || trimmed == "clear") { Clear(); return; }

    if (trimmed == "help" || trimmed == "?")
    {
        Append("  cls / clear   - Clear terminal output");
        Append("  pwd           - Print working directory");
        Append("  cd <dir>      - Change working directory");
        Append("  dir / ls      - List directory contents");
        Append("  <anything>    - Run as system command");
        return;
    }

    if (trimmed == "pwd") { Append(m_WorkingDir); return; }

    if (trimmed.rfind("cd ", 0) == 0 || trimmed == "cd")
    {
        std::string target = (trimmed.size() > 3) ? trimmed.substr(3) : "";
        while (!target.empty() && target.front() == ' ') target.erase(0, 1);
        if (target.empty()) { Append(m_WorkingDir); return; }

        std::error_code ec;
        std::filesystem::path newPath;
        if (target == "..")
            newPath = std::filesystem::path(m_WorkingDir).parent_path();
        else if (target == "." || target == ".\\" || target == "./")
            return;
        else
            newPath = std::filesystem::absolute(target);

        if (std::filesystem::exists(newPath, ec) && std::filesystem::is_directory(newPath, ec))
        {
            m_WorkingDir = newPath.string();
            std::filesystem::current_path(m_WorkingDir, ec);
        }
        else
        {
            char errBuf[512];
            snprintf(errBuf, sizeof(errBuf), "cd: '%s' is not a valid directory", target.c_str());
            Append(errBuf);
        }
        return;
    }

    if (trimmed == "dir" || trimmed == "ls")
    {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(m_WorkingDir, ec))
        {
            std::string name = entry.path().filename().string();
            char ebuf[512];
            if (entry.is_directory())
                snprintf(ebuf, sizeof(ebuf), "  [DIR]  %s", name.c_str());
            else
                snprintf(ebuf, sizeof(ebuf), "         %s", name.c_str());
            Append(ebuf);
        }
        if (ec)
        {
            char errBuf[256];
            snprintf(errBuf, sizeof(errBuf), "Error: %s", ec.message().c_str());
            Append(errBuf);
        }
        return;
    }

#ifdef _WIN32
    std::string fullCmd = "cmd /c \"cd /d " + m_WorkingDir + " && " + trimmed + " 2>&1\"";
    FILE* pipe = _popen(fullCmd.c_str(), "r");
#else
    std::string fullCmd = "cd \"" + m_WorkingDir + "\" && " + trimmed + " 2>&1";
    FILE* pipe = popen(fullCmd.c_str(), "r");
#endif

    if (!pipe) { Append("Error: failed to execute command"); return; }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        std::string line(buffer);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        Append(line);
    }
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
}

void TerminalPanel::ReclaimFocus()
{
    m_FocusInput = true;
}

// ============================================================================
// Draw — follows RobotStatus pattern: simple Begin/End, no clipper, no child
// ============================================================================
void TerminalPanel::Draw(bool* pOpen)
{
    if (!pOpen || !*pOpen) return;

    if (!ImGui::Begin("Terminal", pOpen))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Clear Terminal"))
        Clear();
    ImGui::SameLine();
    ImGui::TextDisabled("|  CWD: %s", m_WorkingDir.c_str());
    ImGui::Separator();

    // Snapshot lines under lock, then render unlocked
    std::deque<TerminalLine> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        snapshot = m_Lines;
    }

    for (const auto& line : snapshot)
    {
        if (line.isInput)
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "%s", line.text.c_str());
        else
            ImGui::TextUnformatted(line.text.c_str());
    }

    ImGui::Separator();

    // Input line
    ImGui::TextUnformatted(">");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);

    if (m_FocusInput)
    {
        ImGui::SetKeyboardFocusHere();
        m_FocusInput = false;
    }

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue
                              | ImGuiInputTextFlags_CallbackHistory;

    auto historyCallback = [](ImGuiInputTextCallbackData* data) -> int
    {
        auto* self = (TerminalPanel*)data->UserData;
        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
        {
            int prevPos  = self->m_HistoryPos;
            int maxIndex = (int)self->m_History.size() - 1;

            if (data->EventKey == ImGuiKey_UpArrow && prevPos < maxIndex)
                self->m_HistoryPos = prevPos + 1;
            else if (data->EventKey == ImGuiKey_DownArrow && prevPos >= 0)
                self->m_HistoryPos = prevPos - 1;

            if (self->m_HistoryPos != prevPos)
            {
                const char* str = (self->m_HistoryPos >= 0)
                    ? self->m_History[self->m_HistoryPos].c_str() : "";
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, str);
            }
        }
        return 0;
    };

    if (ImGui::InputText("##TermInput", m_InputBuf, sizeof(m_InputBuf),
                         flags, historyCallback, this))
    {
        std::string cmd(m_InputBuf);
        if (!cmd.empty())
        {
            if (m_History.empty() || m_History[0] != cmd)
                m_History.insert(m_History.begin(), cmd);
            while (m_History.size() > MaxHistory)
                m_History.pop_back();
            m_HistoryPos = -1;

            ExecuteCommand(cmd);
        }
        memset(m_InputBuf, 0, sizeof(m_InputBuf));
        ReclaimFocus();
    }

    if (ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive() &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        ReclaimFocus();
    }

    ImGui::End();
}
