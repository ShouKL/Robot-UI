#include "NotificationPanel.h"
#include "Walnut/Core/Log.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>

// ============================================================================
// Color mapping
// ============================================================================
static ImVec4 LevelToColor(spdlog::level::level_enum lvl)
{
    switch (lvl)
    {
    case spdlog::level::trace:    return ImVec4(0.60f, 0.60f, 0.60f, 1.0f);  // gray
    case spdlog::level::debug:    return ImVec4(0.40f, 0.55f, 0.80f, 1.0f);  // blue
    case spdlog::level::info:     return ImVec4(0.85f, 0.85f, 0.85f, 1.0f);  // white
    case spdlog::level::warn:     return ImVec4(1.00f, 0.85f, 0.30f, 1.0f);  // yellow
    case spdlog::level::err:      return ImVec4(1.00f, 0.35f, 0.35f, 1.0f);  // red
    case spdlog::level::critical: return ImVec4(1.00f, 0.15f, 0.15f, 1.0f);  // dark red
    default:                      return ImVec4(0.85f, 0.85f, 0.85f, 1.0f);  // white
    }
}

// ============================================================================
// Clear
// ============================================================================
void NotificationPanel::Clear()
{
    Walnut::Log::ClearLogEntries();
}

// ============================================================================
// Draw — reads from Walnut::Log::GetLogEntries() (populated by ImGuiLogSink)
// ============================================================================
void NotificationPanel::Draw(bool* pOpen)
{
    if (!pOpen || !*pOpen) return;

    if (!ImGui::Begin("Output", pOpen))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Clear Output"))
        Clear();
    ImGui::SameLine();
    {
        auto& entries = Walnut::Log::GetLogEntries();
        ImGui::Text("Lines: %zu", entries.size());
    }

    // ---- Filters ----
    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputTextWithHint("##Filter", "keyword filter...", m_FilterBuf, sizeof(m_FilterBuf));
    ImGui::SameLine();
    ImGui::Checkbox("T", &m_ShowTrace);     ImGui::SameLine();
    ImGui::Checkbox("D", &m_ShowDebug);     ImGui::SameLine();
    ImGui::Checkbox("I", &m_ShowInfo);      ImGui::SameLine();
    ImGui::Checkbox("W", &m_ShowWarn);      ImGui::SameLine();
    ImGui::Checkbox("E", &m_ShowError);     ImGui::SameLine();
    ImGui::Checkbox("C", &m_ShowCritical);

    ImGui::Separator();

    // Build filtered snapshot
    std::string filterKW(m_FilterBuf);
    bool hasFilter = !filterKW.empty();

    auto levelOK = [&](spdlog::level::level_enum lvl) -> bool
    {
        switch (lvl)
        {
        case spdlog::level::trace:    return m_ShowTrace;
        case spdlog::level::debug:    return m_ShowDebug;
        case spdlog::level::info:     return m_ShowInfo;
        case spdlog::level::warn:     return m_ShowWarn;
        case spdlog::level::err:      return m_ShowError;
        case spdlog::level::critical: return m_ShowCritical;
        default: return true;
        }
    };

    std::deque<LogLine> snapshot;
    {
        std::lock_guard<std::mutex> lock(Walnut::Log::GetLogMutex());
        for (const auto& entry : Walnut::Log::GetLogEntries())
        {
            if (!levelOK(entry.level)) continue;
            if (hasFilter)
            {
                auto it = std::search(entry.message.begin(), entry.message.end(),
                    filterKW.begin(), filterKW.end(),
                    [](char a, char b) { return std::tolower(a) == std::tolower(b); });
                if (it == entry.message.end()) continue;
            }
            snapshot.push_back({entry.message, LevelToColor(entry.level)});
        }
    }

    bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();

    for (const auto& line : snapshot)
        ImGui::TextColored(line.color, "%s", line.text.c_str());

    if (atBottom)
        ImGui::SetScrollHereY(1.0f);

    ImGui::End();
}
