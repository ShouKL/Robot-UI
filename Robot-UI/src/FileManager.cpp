#include "FileManager.h"

#include <algorithm>

FileManager::FileManager()
{
}

// ==================== .kernel 路径推导 ====================

std::string FileManager::DeriveKernelPath() const
{
    return GetExeDir() + "..\\..\\..\\asset\\file\\default.kernel";
}

// ==================== Win32 文件对话框 ====================

std::string FileManager::OpenDialog(const char* filter)
{
    HWND hwnd = GetActiveWindow();
    char filePath[MAX_PATH] = "";
    std::string exeDir = GetExeDir();

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter ? filter : "All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = sizeof(filePath);
    ofn.lpstrInitialDir = exeDir.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn))
        return filePath;
    return "";
}

std::string FileManager::SaveDialog(const char* filter, const char* defaultExt)
{
    HWND hwnd = GetActiveWindow();
    char filePath[MAX_PATH] = "";
    std::string exeDir = GetExeDir();

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter ? filter : "All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = sizeof(filePath);
    ofn.lpstrDefExt = defaultExt;
    ofn.lpstrInitialDir = exeDir.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    if (GetSaveFileNameA(&ofn))
        return filePath;
    return "";
}

// ==================== 最近文件 ====================

void FileManager::AddRecentFile(const std::string& path)
{
    if (path.empty()) return;

    auto it = std::find(m_RecentFiles.begin(), m_RecentFiles.end(), path);
    if (it != m_RecentFiles.end())
        m_RecentFiles.erase(it);

    m_RecentFiles.insert(m_RecentFiles.begin(), path);

    if ((int)m_RecentFiles.size() > MaxRecentFiles)
        m_RecentFiles.resize(MaxRecentFiles);
}

void FileManager::RemoveRecentFile(const std::string& path)
{
    auto it = std::find(m_RecentFiles.begin(), m_RecentFiles.end(), path);
    if (it != m_RecentFiles.end())
        m_RecentFiles.erase(it);
}

// ==================== 关闭确认 ====================

bool FileManager::ConfirmDiscardChanges(const char* title) const
{
    if (!IsAnyDirty())
        return true;

    int result = MessageBoxA(
        GetActiveWindow(),
        "You have unsaved changes. Do you want to discard them?",
        title,
        MB_YESNO | MB_ICONWARNING);

    return (result == IDYES);
}

// ==================== 工具 ====================

std::string FileManager::GetExeDir()
{
    char exePath[MAX_PATH] = "";
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
    size_t pos = std::string(exePath).find_last_of("\\/");
    if (pos != std::string::npos)
        return std::string(exePath).substr(0, pos + 1);
    return "";
}
