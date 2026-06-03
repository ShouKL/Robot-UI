#include "OptionPanel.h"
#include "Walnut/Core/Log.h"
#include "imgui.h"
#include <algorithm>

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

            const char* items[] = { "Style" };
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
                if (m_ImGuiStyleManager) {
                    m_ImGuiStyleManager->DrawStylePanel();
                }
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

    EditDraftBase::CancelEdit();
}

void OptionPanel::TakeSnapshots()
{
    if (m_ImGuiStyleManager) {
        m_StyleSnapshot_Theme  = m_ImGuiStyleManager->GetTheme();
        m_StyleSnapshot_Invert = m_ImGuiStyleManager->GetInvert();
        m_StyleSnapshot_Alpha  = m_ImGuiStyleManager->GetAlpha();
    }
}
