#pragma once

#include <algorithm>   // workaround: ImTerm misc.hpp uses std::transform without including <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "imterm/terminal.hpp"
#include "imterm/terminal_helpers.hpp"

// State shared between commands
struct RobotTerminalState
{
    bool should_close = false;
};

class TerminalCommands : public ImTerm::basic_spdlog_terminal_helper<
    TerminalCommands,
    RobotTerminalState,
    std::mutex>
{
public:
    TerminalCommands();

    static std::vector<std::string> no_completion(argument_type&) { return {}; }

    static void clear(argument_type& arg) { arg.term.clear(); }
    static void help(argument_type& arg);
    static void quit(argument_type& arg);

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override;
};

// Single panel: merges Terminal input + Output log (spdlog sink)
class TerminalPanel
{
public:
    TerminalPanel();
    ~TerminalPanel() = default;

    void Draw(bool* pOpen);
    void Clear();

    /// Call once to route all spdlog output into this panel.
    void InstallAsLogSink();

private:
    void ExecuteShellCommand(const std::string& cmd);

    RobotTerminalState m_State;
    std::unique_ptr<ImTerm::terminal<TerminalCommands>> m_Terminal;
    size_t m_LastHistorySize = 0;
};
