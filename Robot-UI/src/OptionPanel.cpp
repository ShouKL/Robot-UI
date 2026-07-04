#include "OptionPanel.h"
#include "MonitorWall.h"
#include "Screenshot.h"
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

            const char* items[] = { "Shortcuts", "Style", "Display", "Screenshot", "Connection" };
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

            if (m_SelectedId == 0) {
                DrawShortcutsPanel();
            } else if (m_SelectedId == 1) {
                if (m_ImGuiStyleManager) {
                    m_ImGuiStyleManager->DrawStylePanel();
                }
            } else if (m_SelectedId == 2) {
                DrawDisplayPanel();
            } else if (m_SelectedId == 3) {
                DrawScreenshotPanel();
            } else if (m_SelectedId == 4) {
                DrawConnectionPanel();
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
    m_ConnRetryCount  = m_ConnRetryCountSnapshot;
    m_CameraRetryCount = m_CameraRetryCountSnapshot;

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
    m_ConnRetryCountSnapshot  = m_ConnRetryCount;
    m_CameraRetryCountSnapshot = m_CameraRetryCount;
}

// ==================== 快捷键面板（只读参考）====================

void OptionPanel::DrawShortcutsPanel()
{
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Keyboard Shortcuts");
    ImGui::Spacing();
    ImGui::TextWrapped("Click on a key binding to rebind it");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!m_ShortcutMgr) { ImGui::TextDisabled("ShortcutManager not available."); return; }

    int actionCount = m_ShortcutMgr->GetActionCount();

    // Handle key capture when in rebinding mode
    if (m_RebindingAction >= 0 && m_RebindingAction < actionCount)
    {
        auto& io = ImGui::GetIO();

        // Wait for ALL keys to be released first (avoids capturing the click itself)
        if (m_RebindingWaitingRelease)
        {
            bool anyKeyDown = false;
            for (ImGuiKey k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; k = (ImGuiKey)((int)k + 1))
                if (ImGui::IsKeyDown(k)) { anyKeyDown = true; break; }
            if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt && !anyKeyDown)
                m_RebindingWaitingRelease = false;
        }
        else
        {
            // Capture the first non-modifier key press + current modifier state
            for (int ki = 0; ki < (int)ShortcutManager::GetBindableKeys().size(); ++ki)
            {
                ImGuiKey key = ShortcutManager::GetBindableKeys()[ki];

                // Skip modifier keys — they serve as ctrl/shift/alt flags, not bindable keys
                if (key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl ||
                    key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift ||
                    key == ImGuiKey_LeftAlt || key == ImGuiKey_RightAlt)
                    continue;

                if (ImGui::IsKeyPressed(key, false))
                {
                    auto& b = m_ShortcutMgr->GetBinding(m_RebindingAction);
                    b.key   = key;
                    b.ctrl  = io.KeyCtrl;
                    b.shift = io.KeyShift;
                    b.alt   = io.KeyAlt;
                    m_RebindingAction = -1;
                    m_RebindingWaitingRelease = true;
                    break;
                }
            }
        }

        // Escape to cancel rebinding
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        {
            m_RebindingAction = -1;
            m_RebindingWaitingRelease = true;
        }
    }

    if (ImGui::BeginTable("ShortcutsTable", 4,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Category",   ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Action",     ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Keys",       ImGuiTableColumnFlags_WidthFixed, 200);
        ImGui::TableSetupColumn("",           ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableHeadersRow();

        const char* lastCategory = nullptr;
        for (int i = 0; i < actionCount; ++i)
        {
            auto& b = m_ShortcutMgr->GetBinding(i);
            const char* cat = ShortcutManager::GetActionCategory(i);
            const char* lbl = ShortcutManager::GetActionLabel(i);
            bool isRebinding = (m_RebindingAction == i);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (lastCategory != cat) {
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "%s", cat);
                lastCategory = cat;
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(lbl);

            ImGui::TableSetColumnIndex(2);
            if (isRebinding)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "Press a key... (Esc to cancel)");
            }
            else
            {
                std::string keyStr = b.ToString();
                ImGui::PushID(i);
                if (ImGui::Selectable(keyStr.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
                {
                    m_RebindingAction = i;
                    m_RebindingWaitingRelease = true;  // wait for all keys released first
                }
                ImGui::PopID();
            }

            // Reset button
            ImGui::TableSetColumnIndex(3);
            ImGui::PushID(i + 1000);
            if (ImGui::SmallButton("R")) {
                m_ShortcutMgr->GetBinding(i) = ShortcutManager::GetDefaultBinding(i);
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Reset to default");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
}

// ==================== Display Panel ====================

void OptionPanel::DrawDisplayPanel()
{
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Monitor Wall");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!m_MonitorWall) {
        ImGui::TextDisabled("MonitorWall not available.");
        return;
    }

    bool flipH = m_MonitorWall->GetFlipH();
    bool flipV = m_MonitorWall->GetFlipV();
    float angle = m_MonitorWall->GetRotationAngle();

    ImGui::Checkbox("Flip Horizontal", &flipH);
    ImGui::Checkbox("Flip Vertical",   &flipV);
    ImGui::SetNextItemWidth(150.0f);
    ImGui::SliderFloat("Rotation Angle", &angle, -360.0f, 360.0f, "%.0f deg");

    if (flipH != m_MonitorWall->GetFlipH()) m_MonitorWall->SetFlipH(flipH);
    if (flipV != m_MonitorWall->GetFlipV()) m_MonitorWall->SetFlipV(flipV);
    if (angle != m_MonitorWall->GetRotationAngle()) m_MonitorWall->SetRotationAngle(angle);

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

    char pathBuf[512] = {};
    strncpy_s(pathBuf, m_ScreenshotPath.c_str(), sizeof(pathBuf) - 1);

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
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "Press F12 to take a screenshot");
    ImGui::Spacing();
}

// ==================== Connection Panel ====================

void OptionPanel::DrawConnectionPanel()
{
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Connection Settings");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextUnformatted("Robot Retry Count");
    ImGui::SameLine();
    ImGui::SetCursorPosX(200.0f);
    ImGui::SetNextItemWidth(200.0f);
    ImGui::SliderInt("##RobotRetryCount", &m_ConnRetryCount, 1, 20);
    ImGui::Spacing();
    ImGui::TextDisabled("Total attempts (including the first) when linking to robot.");
    ImGui::TextDisabled("Each retry waits ~0.8s.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextUnformatted("Camera Retry Count");
    ImGui::SameLine();
    ImGui::SetCursorPosX(200.0f);
    ImGui::SetNextItemWidth(200.0f);
    ImGui::SliderInt("##CamRetryCount", &m_CameraRetryCount, 0, 10);
    ImGui::Spacing();
    ImGui::TextDisabled("Extra retries after the first attempt when connecting to camera.");
    ImGui::TextDisabled("0 = try once. Uses background thread, won't block UI.");
    ImGui::Spacing();
}
