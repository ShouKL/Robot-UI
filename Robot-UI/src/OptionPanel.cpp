#include "OptionPanel.h"
#include "Walnut/Core/Log.h"
#include <algorithm>

OptionPanel::OptionPanel()
{
    m_GamepadMapper = std::make_unique<GamepadMapper>();
    m_ImGuiStyleManager = std::make_unique<ImGuiStyleManager>();
    m_RobotComponent = std::make_unique<RobotComponent>();
}

OptionPanel::~OptionPanel() {}

void OptionPanel::DrawOptionPanel()
{
    float footerHeight = ImGui::GetFrameHeightWithSpacing() + 5.0f;
    float availableHeight = ImGui::GetContentRegionAvail().y - footerHeight;

    if (ImGui::BeginTable("MainLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Sidebar", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("SubSidebar", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        // ---- 左侧大类别选择 ----
        ImGui::TableSetColumnIndex(0);
        if (ImGui::BeginChild("SideBarChild", ImVec2(0, availableHeight), true))
        {
            // 首次打开 Options 时统一拍照
            if (!IsEditing())
                BeginEdit();

            const char* items[] = { "Component", "GamepadMapper", "Style" };
            int previous_id = selected_id;
            for (int i = 0; i < IM_ARRAYSIZE(items); i++) {
                ImGui::PushID(i);
                if (ImGui::Selectable(items[i], selected_id == i, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 30))) {
                    selected_id = i;
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();

        // ---- 子侧边栏（模式列表） ----
        ImGui::TableSetColumnIndex(1);
        if (ImGui::BeginChild("SubSideBarChild", ImVec2(0, availableHeight), true))
        {
            if (selected_id == 0) {
                if (m_RobotComponent) {
                    auto& modes = m_RobotComponent->GetModes();
                    int& editIdx = m_RobotComponent->GetEditModeIndex();
                    for (int i = 0; i < (int)modes.size(); ++i) {
                        ImGui::PushID(i);
                        bool isSel = (editIdx == i);
                        if (ImGui::Selectable(modes[i].name, isSel, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 30)))
                            editIdx = i;
                        if (modes.size() > 1) {
                            if (ImGui::BeginPopupContextItem()) {
                                if (ImGui::MenuItem("Delete Mode"))
                                    m_RobotComponent->DeleteMode(i);
                                ImGui::EndPopup();
                            }
                        }
                        ImGui::PopID();
                    }
                    if (ImGui::BeginPopupContextWindow("RobotModeListPopup", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                        if (ImGui::MenuItem("Add Mode"))
                            m_RobotComponent->AddMode();
                        ImGui::EndPopup();
                    }
                }
            }

            // ------ 手柄映射模式列表 ------
            if (selected_id == 1 && m_GamepadMapper) {
                auto& modes = m_GamepadMapper->GetModes();
                int curIdx = m_GamepadMapper->GetSelectedModeIndex();
                for (int i = 0; i < (int)modes.size(); ++i) {
                    ImGui::PushID(i);
                    bool isSelected = (curIdx == i);
                    if (ImGui::Selectable(modes[i].name, isSelected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 30))) {
                        m_GamepadMapper->SetSelectedModeIndex(i);
                    }
                    if (modes.size() > 1) {
                        if (ImGui::BeginPopupContextItem()) {
                            if (ImGui::MenuItem("Delete Mode")) {
                                m_GamepadMapper->DeleteMode(i);
                            }
                            ImGui::EndPopup();
                        }
                    }
                    ImGui::PopID();
                }
                if (ImGui::BeginPopupContextWindow("GamepadModeListPopup", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                    if (ImGui::MenuItem("Add Mode")) {
                        m_GamepadMapper->AddMode();
                    }
                    ImGui::EndPopup();
                }
            }
        }
        ImGui::EndChild();

        // ---- 右侧内容区 ----
        ImGui::TableSetColumnIndex(2);
        if (ImGui::BeginChild("DetailsChild", ImVec2(0, availableHeight), false))
        {
            ImGui::Indent(10.0f);
            ImGui::Spacing();

            if (selected_id == 0) {
                if (m_RobotComponent) {
                    m_RobotComponent->DrawRobotConfigPanel();
                }
                else {
                    ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "RobotComponent not connected.");
                }
            }
            else if (selected_id == 1) {
                if (m_GamepadMapper) {
                    m_GamepadMapper->DrawGamepadMapper();
                }
            }
            else if (selected_id == 2) {
                if (m_ImGuiStyleManager) {
                    m_ImGuiStyleManager->DrawStylePanel();
                }
            }

            ImGui::Unindent(10.0f);
        }
        ImGui::EndChild();

        ImGui::EndTable();
    }
}

void OptionPanel::BeginEdit()
{
    if (IsEditing()) return;
    EditDraftBase::BeginEdit();  // 设置 m_IsEditing = true
    TakeSnapshots();
    WL_INFO_TAG("CONFIG", "Options editing started");
}

void OptionPanel::ApplyEdit()
{
    WL_INFO_TAG("CONFIG", "Applying configuration...");

    // 统一：三个 tab 的修改已在原数据上，清快照即可
    m_ComponentSnapshot.clear();
    m_GamepadSnapshot.clear();
    // RobotStatus 持有指向 RobotComponent 中模式的指针，修改自动生效，无需额外同步
    EditDraftBase::ApplyEdit();
}

void OptionPanel::CancelEdit()
{
    WL_INFO_TAG("CONFIG", "Reverting configuration...");

    // 恢复 Component 快照
    if (m_RobotComponent && !m_ComponentSnapshot.empty()) {
        m_RobotComponent->GetModes() = m_ComponentSnapshot;
        m_ComponentSnapshot.clear();
    }

    // 恢复 Gamepad 快照
    if (m_GamepadMapper && !m_GamepadSnapshot.empty()) {
        m_GamepadMapper->GetModes() = m_GamepadSnapshot;
        m_GamepadSnapshot.clear();
    }

    // 恢复 Style 快照
    if (m_ImGuiStyleManager) {
        m_ImGuiStyleManager->ApplyImGuiStyle(
            m_StyleSnapshot_Theme, m_StyleSnapshot_Invert, m_StyleSnapshot_Alpha);
    }

    EditDraftBase::CancelEdit();
}

// ==================== 快照 ====================

void OptionPanel::TakeSnapshots()
{
    if (m_RobotComponent)
        m_ComponentSnapshot = m_RobotComponent->GetModes();

    if (m_GamepadMapper)
        m_GamepadSnapshot = m_GamepadMapper->GetModes();

    if (m_ImGuiStyleManager) {
        m_StyleSnapshot_Theme  = m_ImGuiStyleManager->GetTheme();
        m_StyleSnapshot_Invert = m_ImGuiStyleManager->GetInvert();
        m_StyleSnapshot_Alpha  = m_ImGuiStyleManager->GetAlpha();
    }
}