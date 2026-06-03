#pragma once

#include <deque>
#include <mutex>
#include <string>
#include <imgui.h>

struct LogLine
{
    std::string text;
    ImVec4      color;
};

class NotificationPanel
{
public:
    static constexpr size_t MaxLines = 2000;

    /// No-op now — the sink is registered inside Walnut::Log::Init() directly.
    static void InstallLogSink() {}

    void Clear();
    void Draw(bool* pOpen);

private:
    char    m_FilterBuf[128]  = {};
    bool    m_ShowTrace       = true;
    bool    m_ShowDebug       = true;
    bool    m_ShowInfo        = true;
    bool    m_ShowWarn        = true;
    bool    m_ShowError       = true;
    bool    m_ShowCritical    = true;
};
