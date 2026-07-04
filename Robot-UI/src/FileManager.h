#pragma once

#include "core.h"
#include <commdlg.h>
#include <filesystem>

// ============================================================================
// FileManager — 纯工具类，无业务耦合
// 职责：路径追踪、脏状态、Win32 文件对话框、最近文件列表
// 序列化逻辑由 Robot_UI_Layer 自行处理
// ============================================================================

class FileManager
{
public:
    FileManager();
    ~FileManager() = default;

    // ---- 路径管理 ----
    const std::string& GetRobotPath()  const { return m_RobotPath; }
    void  SetRobotPath(const std::string& path) { m_RobotPath = path; }
    bool  HasRobotPath() const { return !m_RobotPath.empty(); }

    // .kernel 路径由 .rbt 推导：同目录同主名 .kernel 扩展名
    std::string DeriveKernelPath() const;

    // ---- 脏状态 ----
    bool IsRobotDirty()  const { return m_RobotDirty; }
    bool IsKernelDirty() const { return m_KernelDirty; }
    bool IsAnyDirty()    const { return m_RobotDirty || m_KernelDirty; }
    void MarkRobotDirty()  { m_RobotDirty = true; }
    void MarkKernelDirty() { m_KernelDirty = true; }
    void MarkRobotClean()  { m_RobotDirty = false; }
    void MarkKernelClean() { m_KernelDirty = false; }

    // ---- Win32 文件对话框 ----
    static std::string OpenDialog(const char* filter = nullptr);
    static std::string SaveDialog(const char* filter = nullptr, const char* defaultExt = "rbt");

    // ---- 最近文件列表 ----
    static constexpr int MaxRecentFiles = 10;
    const std::vector<std::string>& GetRecentFiles() const { return m_RecentFiles; }
    void AddRecentFile(const std::string& path);
    void ClearRecentFiles() { m_RecentFiles.clear(); }
    void RemoveRecentFile(const std::string& path);
    void SetRecentFiles(const std::vector<std::string>& files) { m_RecentFiles = files; }

    // ---- 脏状态设置（供 .kernel 加载后恢复） ----
    void SetRobotDirty(bool v)  { m_RobotDirty = v; }
    void SetKernelDirty(bool v) { m_KernelDirty = v; }

    // ---- 安全关闭确认 ----
    bool ConfirmDiscardChanges(const char* title = "Unsaved Changes") const;

    // ---- 工具 ----
    static std::string GetExeDir();

    // ---- 路径便携化：绝对 <-> 相对（相对于 exe 目录）----
    // 将绝对路径转为相对于 exe 目录的路径（仅当路径在 exe 目录子树下时）；
    // 如果路径不在 exe 目录下（如用户从桌面打开），则保持原样。
    static std::string ToRelativePath(const std::string& absolutePath);
    // 将相对路径还原为绝对路径（基于当前 exe 目录）。
    // 如果路径已经是绝对路径，则直接返回。
    static std::string ToAbsolutePath(const std::string& path);

private:
    std::string m_RobotPath;
    bool m_RobotDirty  = false;
    bool m_KernelDirty = false;
    std::vector<std::string> m_RecentFiles;
};
