#pragma once

#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include <imgui.h>

struct TerminalLine
{
    std::string text;
    bool isInput = false;
};

class TerminalPanel
{
public:
    static constexpr size_t MaxLines   = 2000;
    static constexpr size_t MaxHistory = 50;

    TerminalPanel();

    void Draw(bool* pOpen);
    void Clear();

private:
    void ExecuteCommand(const std::string& cmd);
    void Append(const std::string& text, bool isInput = false);
    void ReclaimFocus();

    std::deque<TerminalLine>    m_Lines;
    std::mutex                  m_Mutex;
    std::vector<std::string>    m_History;
    int                         m_HistoryPos   = -1;
    char                        m_InputBuf[512] = {};
    bool                        m_FocusInput    = false;
    std::string                 m_WorkingDir;
};
