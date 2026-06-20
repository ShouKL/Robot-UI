#include "OptionPanel.h"
#include "Screenshot.h"
#include "Walnut/Core/Log.h"
#include "FileManager.h"
#include "imgui.h"
#include <algorithm>
#include <windows.h>
#include <shlobj.h>

OptionPanel::OptionPanel()
{
    m_ImGuiStyleManager = std::make_unique<ImGuiStyleManager>();
}

OptionPanel::~OptionPanel() {}

void OptionPanel::DrawOptionPanel(bool* p_open)
{
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(displaySize.x * 0.85f, displaySize.y * 0.8f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(400, 300),
        ImVec2(displaySize.x, displaySize.y));
    if (!ImGui::Begin("Option", p_open, ImGuiWindowFlags_AlwaysVerticalScrollbar))
    {
        ImGui::End();
        return;
    }

    float footerHeight = ImGui::GetFrameHeightWithSpacing() + 5.0f;
    float availableHeight = ImGui::GetContentRegionAvail().y - footerHeight;

    if (ImGui::BeginTable("MainLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Sidebar", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        // ---- 左侧大类别选择 ----
        ImGui::TableSetColumnIndex(0);
        if (ImGui::BeginChild("SideBarChild", ImVec2(0, availableHeight), true))
        {
            if (!IsEditing())
                BeginEdit();

            const char* items[] = { "Shortcuts", "Style", "Screenshot" };
            for (int i = 0; i < IM_ARRAYSIZE(items); i++) {
                ImGui::PushID(i);
                if (ImGui::Selectable(items[i], m_SelectedId == i, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 30))) {
                    m_SelectedId = i;
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
        }

        // ---- 右侧内容区 ----
        ImGui::TableSetColumnIndex(1);
        if (ImGui::BeginChild("DetailsChild", ImVec2(0, availableHeight), false))
        {
            ImGui::Indent(10.0f);
            ImGui::Spacing();

            if (m_SelectedId == 1) {
                if (m_ImGuiStyleManager) {
                    m_ImGuiStyleManager->DrawStylePanel();
                }
            } else if (m_SelectedId == 0) {
                DrawShortcutsPanel();
            } else if (m_SelectedId == 2) {
                DrawScreenshotPanel();
            }

            ImGui::Unindent(10.0f);
            ImGui::EndChild();
        }

        ImGui::EndTable();
    }

    float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

    if (ImGui::Button("Apply", ImVec2(buttonWidth, 0)))
        ApplyEdit();

    ImGui::SameLine();

    if (ImGui::Button("Close##2", ImVec2(buttonWidth, 0)))
    {
        if (p_open) *p_open = false;
        CancelEdit();
    }

    ImGui::End();
}

bool OptionPanel::IsRobotSettingRequested() const { return m_OpenRobotSettingRequested; }
void OptionPanel::ClearRobotSettingRequest() { m_OpenRobotSettingRequested = false; }

void OptionPanel::BeginEdit()
{
    if (IsEditing()) return;
    EditDraftBase::BeginEdit();
    TakeSnapshots();
    WL_INFO_TAG("component", "Options editing started");
}

void OptionPanel::ApplyEdit()
{
    WL_INFO_TAG("component", "Applying configuration...");

    if (m_ImGuiStyleManager) {
        m_ImGuiStyleManager->ApplyActiveStyle();
    }

    EditDraftBase::ApplyEdit();
}

void OptionPanel::CancelEdit()
{
    WL_INFO_TAG("component", "Reverting configuration...");

    if (m_ImGuiStyleManager) {
        m_ImGuiStyleManager->ApplyImGuiStyle(
            m_StyleSnapshot_Theme, m_StyleSnapshot_Invert, m_StyleSnapshot_Alpha);
    }
    m_ScreenshotScope = m_ScreenshotScopeSnapshot;
    m_ScreenshotPath  = m_ScreenshotPathSnapshot;

    EditDraftBase::CancelEdit();
}

void OptionPanel::TakeSnapshots()
{
    if (m_ImGuiStyleManager) {
        m_StyleSnapshot_Theme  = m_ImGuiStyleManager->GetTheme();
        m_StyleSnapshot_Invert = m_ImGuiStyleManager->GetInvert();
        m_StyleSnapshot_Alpha  = m_ImGuiStyleManager->GetAlpha();
    }
    m_ScreenshotScopeSnapshot = m_ScreenshotScope;
    m_ScreenshotPathSnapshot  = m_ScreenshotPath;
}

// ==================== 快捷键面板（只读参考）====================

void OptionPanel::DrawShortcutsPanel()
{
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Keyboard Shortcuts");
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.3f, 1.0f));
    ImGui::TextWrapped("Shortcuts are fixed and shown in the menu bar. This table is for reference only.");
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!m_ShortcutMgr) { ImGui::TextDisabled("ShortcutManager not available."); return; }

    int actionCount = m_ShortcutMgr->GetActionCount();

    if (ImGui::BeginTable("ShortcutsTable", 3,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Category",   ImGuiTableColumnFlags_WidthFixed, 160);
        ImGui::TableSetupColumn("Action",     ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Keys",       ImGuiTableColumnFlags_WidthFixed, 220);
        ImGui::TableHeadersRow();

        const char* lastCategory = nullptr;
        for (int i = 0; i < actionCount; ++i)
        {
            auto& b = m_ShortcutMgr->GetBinding(i);
            const char* cat = ShortcutManager::GetActionCategory(i);
            const char* lbl = ShortcutManager::GetActionLabel(i);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (lastCategory != cat) {
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "%s", cat);
                lastCategory = cat;
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(lbl);

            ImGui::TableSetColumnIndex(2);
            ImGui::TextDisabled("%s", b.ToString().c_str());
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
}

// ==================== 截图设置面板 ====================

void OptionPanel::DrawScreenshotPanel()
{
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Screenshot Settings");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ---- 截图范围 ----
    ImGui::TextUnformatted("Capture Scope");
    ImGui::SameLine();
    ImGui::SetCursorPosX(200.0f);
    const char* const* scopeItems = Screenshot::GetWindowNames();
    int scopeCount = Screenshot::COUNT;
    if (ImGui::Combo("##Scope", &m_ScreenshotScope, scopeItems, scopeCount))
    {
    }
    ImGui::Spacing();

    // ---- 保存路径 ----
    ImGui::TextUnformatted("Save Path");
    ImGui::SameLine();
    ImGui::SetCursorPosX(200.0f);

    char pathBuf[512];
    strncpy(pathBuf, m_ScreenshotPath.c_str(), sizeof(pathBuf) - 1);
    pathBuf[sizeof(pathBuf) - 1] = '\0';

    float inputWidth = ImGui::GetContentRegionAvail().x - 90.0f;
    ImGui::PushItemWidth(inputWidth);
    if (ImGui::InputText("##Path", pathBuf, sizeof(pathBuf)))
        m_ScreenshotPath = pathBuf;
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::Button("Browse..."))
    {
        BROWSEINFOW bi = {};
        bi.lpszTitle = L"Select Screenshot Folder";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
        if (pidl)
        {
            wchar_t folderPath[MAX_PATH];
            if (SHGetPathFromIDListW(pidl, folderPath))
            {
                int len = WideCharToMultiByte(CP_UTF8, 0, folderPath, -1, nullptr, 0, nullptr, nullptr);
                std::string path(len - 1, '\0');
                WideCharToMultiByte(CP_UTF8, 0, folderPath, -1, &path[0], len, nullptr, nullptr);
                m_ScreenshotPath = path;
            }
            CoTaskMemFree(pidl);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
        "Main Client: client area  |  Full Window: with title bar  |  Entire Screen: all monitors");
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
        "Panel options capture individual ImGui windows");
    ImGui::Spacing();

    // 显示当前实际保存路径
    std::string actualPath = m_ScreenshotPath.empty()
        ? FileManager::GetExeDir() + "..\\..\\asset\\screenshots\\"
        : m_ScreenshotPath;
    if (!actualPath.empty() && actualPath.back() != '\\')
        actualPath += '\\';
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Default (empty): asset/screenshots/");
    ImGui::TextDisabled("Files will be saved to: %s", actualPath.c_str());

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "Press Ctrl+Shift+X to take a screenshot");
    ImGui::Spacing();
}
