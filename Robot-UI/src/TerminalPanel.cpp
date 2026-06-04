#include "TerminalPanel.h"
#include "Walnut/Core/Log.h"
#include <array>
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// ============================================================================
// TerminalCommands — only built-in commands; everything else → system shell
// ============================================================================
TerminalCommands::TerminalCommands()
{
    static constexpr std::array<command_type, 3> cmds = {{
        {"clear", "clears the terminal screen", clear, no_completion},
        {"help",  "show this help",             help, no_completion},
        {"quit",  "close the terminal",         quit, no_completion},
    }};

    for (const auto& cmd : cmds)
        add_command_(cmd);
}

void TerminalCommands::help(argument_type& arg)
{
    ImTerm::message msg;
    msg.value = "Built-in: clear, help, quit\n"
                "Anything else runs as a system command (cmd.exe)";
    msg.color_beg = msg.color_end = 0;
    arg.term.add_message(std::move(msg));
}

void TerminalCommands::quit(argument_type& arg)
{
    arg.term.set_should_close();
    arg.val.should_close = true;
}

void TerminalCommands::sink_it_(const spdlog::details::log_msg& msg)
{
    if (msg.level == spdlog::level::off || !terminal_)
        return;

    spdlog::memory_buf_t buff{};
    this->formatter_->format(msg, buff);
    std::string text = fmt::to_string(buff);
    size_t len = text.size();

    terminal_->add_message({ImTerm::details::to_imterm_severity(msg.level),
                            std::move(text), 0, len, false});
}

// ============================================================================
// TerminalPanel — intercepts unknown input → system shell
// ============================================================================
TerminalPanel::TerminalPanel()
{
    m_Terminal = std::make_unique<ImTerm::terminal<TerminalCommands>>(
        m_State, "Terminal", 600, 300);
    m_Terminal->set_min_log_level(ImTerm::message::severity::trace);

    auto& t = m_Terminal->theme();
    t.log_level_colors[ImTerm::message::severity::trace]    .emplace(0.70f, 0.70f, 0.70f, 1.0f);
    t.log_level_colors[ImTerm::message::severity::debug]    .emplace(0.00f, 0.75f, 0.85f, 1.0f);
    t.log_level_colors[ImTerm::message::severity::info]     .emplace(0.50f, 0.95f, 0.50f, 1.0f);
    t.log_level_colors[ImTerm::message::severity::warn]     .emplace(0.95f, 0.90f, 0.20f, 1.0f);
    t.log_level_colors[ImTerm::message::severity::err]      .emplace(0.95f, 0.25f, 0.25f, 1.0f);
    t.log_level_colors[ImTerm::message::severity::critical] .emplace(0.95f, 0.10f, 0.10f, 1.0f);

    InstallAsLogSink();
    m_LastHistorySize = m_Terminal->get_history().size();
}

void TerminalPanel::Draw(bool* pOpen)
{
    if (!pOpen || !*pOpen) return;

    bool showing = m_Terminal->show();

    if (!showing)
    {
        *pOpen = false;
        return;
    }

    // Check for new history entries → unknown commands → system shell
    const auto& history = m_Terminal->get_history();
    if (history.size() > m_LastHistorySize)
    {
        const std::string& lastInput = history.back();
        // Skip built-in commands (they were handled by ImTerm internally)
        if (lastInput != "clear" && lastInput != "help" && lastInput != "quit")
        {
            ExecuteShellCommand(lastInput);
        }
        m_LastHistorySize = history.size();
    }
}

void TerminalPanel::Clear()
{
    if (m_Terminal)
        m_Terminal->clear();
}

void TerminalPanel::InstallAsLogSink()
{
    if (!m_Terminal) return;

    auto helper = m_Terminal->get_terminal_helper();
    if (helper)
        Walnut::Log::GetClientLogger()->sinks().push_back(helper);
}

void TerminalPanel::ExecuteShellCommand(const std::string& cmd)
{
    // Echo
    {
        ImTerm::message msg;
        msg.value = "> " + cmd;
        msg.color_beg = msg.color_end = 0;
        m_Terminal->add_message(std::move(msg));
    }

    // Run via _popen
    std::string output;
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe)
    {
        ImTerm::message msg;
        msg.value = "ERROR: failed to execute";
        msg.color_beg = msg.color_end = 0;
        m_Terminal->add_message(std::move(msg));
        return;
    }

    char buf[1024];
    while (fgets(buf, sizeof(buf), pipe))
        output += buf;

#ifdef _WIN32
    int rc = _pclose(pipe);
#else
    int rc = pclose(pipe);
#endif

    if (rc != 0)
        output += "(exit code: " + std::to_string(rc) + ")";

    // Trim trailing newline
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
        output.pop_back();

    if (!output.empty())
    {
        ImTerm::message msg;
        msg.value = std::move(output);
        msg.color_beg = msg.color_end = 0;
        m_Terminal->add_message(std::move(msg));
    }
}