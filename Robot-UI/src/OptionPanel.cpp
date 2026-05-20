#include "OptionPanel.h"
#include <algorithm>

OptionPanel::OptionPanel()
{
    m_GamepadMapper = std::make_unique<GamepadMapper>();
    m_ImGuiStyleManager = std::make_unique<ImGuiStyleManager>();
    m_RobotConfig = std::make_unique<Robot_Config>();
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
            const char* items[] = { "Robot", "GamepadMapper", "Style" };
            int previous_id = selected_id;
            for (int i = 0; i < IM_ARRAYSIZE(items); i++) {
                ImGui::PushID(i);
                if (ImGui::Selectable(items[i], selected_id == i, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 30))) {
                    selected_id = i;
                }
                ImGui::PopID();
            }

            // 如果离开 GamepadMapper 页，需要结束编辑状态（但不提交，等待 Apply/Cancel）
            if (previous_id == 1 && selected_id != 1 && m_GamepadEditActive) {
                // 注意：不能在这里直接 CancelEdit，因为用户可能还想 Apply。
                // 我们选择保留草稿，在 Apply/Revert 时处理。
                // 若要在离开时自动放弃，可调用 CancelEdit()，但当前设计希望由外部按钮控制。
                // 因此不做任何操作。
            }
        }
        ImGui::EndChild();

        // ---- 子侧边栏（模式列表） ----
        ImGui::TableSetColumnIndex(1);
        if (ImGui::BeginChild("SubSideBarChild", ImVec2(0, availableHeight), true))
        {
            // ------ 机器人模式列表 ------
            if (selected_id == 0 && m_RobotConfig) {
                auto& modes = m_RobotConfig->GetModes();
                int& edit_mode_index = m_RobotConfig->GetEditModeIndex();
                for (int i = 0; i < (int)modes.size(); ++i) {
                    ImGui::PushID(i);
                    bool is_selected = (edit_mode_index == i);
                    if (ImGui::Selectable(modes[i].name, is_selected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 30))) {
                        edit_mode_index = i;
                    }
                    if (modes.size() > 1) {
                        if (ImGui::BeginPopupContextItem()) {
                            if (ImGui::MenuItem("Delete Mode")) {
                                m_RobotConfig->DeleteMode(i);
                                m_RobotConfigChanged = true;
                            }
                            ImGui::EndPopup();
                        }
                    }
                    ImGui::PopID();
                }
                if (ImGui::BeginPopupContextWindow("RobotModeListPopup", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                    if (ImGui::MenuItem("Add Mode")) {
                        m_RobotConfig->AddMode();
                        m_RobotConfigChanged = true;
                    }
                    ImGui::EndPopup();
                }
            }

            // ------ 手柄映射模式列表 ------
            if (selected_id == 1 && m_GamepadMapper) {
                auto& modes = m_GamepadMapper->GetModes();
                int& editIdx = m_GamepadMapper->GetEditModeIndexRef();
                for (int i = 0; i < (int)modes.size(); ++i) {
                    ImGui::PushID(i);
                    bool isSelected = (editIdx == i);
                    if (ImGui::Selectable(modes[i].name, isSelected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 30))) {
                        // 切换到不同模式时，进入新模式的草稿编辑
                        if (editIdx != i) {
                            m_GamepadMapper->BeginEdit(i);
                            m_LastGamepadEditIndex = i;
                            m_GamepadEditActive = true;
                        }
                    }
                    if (modes.size() > 1) {
                        if (ImGui::BeginPopupContextItem()) {
                            if (ImGui::MenuItem("Delete Mode")) {
                                m_GamepadMapper->DeleteMode(i);
                                // 删除后可能需要更新编辑状态
                                if (m_LastGamepadEditIndex == i) {
                                    m_LastGamepadEditIndex = -1;
                                    m_GamepadEditActive = false;
                                }
                            }
                            ImGui::EndPopup();
                        }
                    }
                    ImGui::PopID();
                }
                if (ImGui::BeginPopupContextWindow("GamepadModeListPopup", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                    if (ImGui::MenuItem("Add Mode")) {
                        m_GamepadMapper->AddMode();  // 内部会自动 BeginEdit 新添加的模式
                        m_LastGamepadEditIndex = m_GamepadMapper->GetEditModeIndexRef();
                        m_GamepadEditActive = true;
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

            // 在 DrawOptionPanel() 中
            if (selected_id == 0) {
                if (!m_RobotConfig->IsEditing()) {
                    m_RobotConfig->BeginEdit();          // 进入草稿
                }
                m_RobotConfig->SetAvailableModeNames(m_GamepadMapper->GetModeNames());
                m_RobotConfig->DrawRobotConfigPanel();
            }
            else if (selected_id == 1) {
                if (m_GamepadMapper) {
                    // 如果不在编辑状态，自动进入编辑当前选中的模式
                    if (!m_GamepadMapper->IsEditing()) {
                        int editIdx = m_GamepadMapper->GetEditModeIndexRef();
                        m_GamepadMapper->BeginEdit(editIdx);
                        m_LastGamepadEditIndex = editIdx;
                        m_GamepadEditActive = true;
                    }
                    else {
                        // 确保编辑的模式索引与子侧边栏选中的一致（可能因外部操作变化）
                        int expectedIdx = m_GamepadMapper->GetEditModeIndexRef();
                        if (m_LastGamepadEditIndex != expectedIdx) {
                            m_GamepadMapper->BeginEdit(expectedIdx);
                            m_LastGamepadEditIndex = expectedIdx;
                        }
                    }
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

bool OptionPanel::Apply()
{
    bool configChanged = false;

    // 1. 提交手柄映射编辑
    if (m_GamepadMapper && m_GamepadMapper->IsEditing()) {
        m_GamepadMapper->ApplyEdit();
        m_GamepadEditActive = false;
        m_LastGamepadEditIndex = -1;
    }

    // 2. 应用主题
    if (m_ImGuiStyleManager) {
        m_ImGuiStyleManager->ApplyImGuiStyle(
            m_ImGuiStyleManager->GetTheme(),
            m_ImGuiStyleManager->GetInvert(),
            m_ImGuiStyleManager->GetAlpha());
    }

    // 3. 应用机器人参数
    if (m_RobotConfig && m_RobotConfig->IsEditing()) {
        m_RobotConfig->ApplyEdit();
    }

    // 4. 根据当前活跃的机器人模式，设置手柄活跃映射
    if (m_RobotConfig && m_GamepadMapper) {
        auto& active_modes = m_RobotConfig->GetActiveModes();
        int active_idx = m_RobotConfig->GetActiveModeIndex();
        if (!active_modes.empty() && active_idx < (int)active_modes.size()) {
            const std::string& Mode = active_modes[active_idx].gamepad_mapping_Mode;
            if (m_GamepadMapper->ModeExists(Mode)) {
                m_GamepadMapper->SetActiveMode(Mode);
            }
        }
    }

    return configChanged;
}

void OptionPanel::Revert()
{
    // 1. 撤销手柄映射编辑
    if (m_GamepadMapper && m_GamepadMapper->IsEditing()) {
        m_GamepadMapper->CancelEdit();
        m_GamepadEditActive = false;
        m_LastGamepadEditIndex = -1;
    }

    // 2. 恢复主题
    if (m_ImGuiStyleManager) {
        m_ImGuiStyleManager->RevertStyle();
    }

    // 3. 恢复机器人配置
    if (m_RobotConfig && m_RobotConfig->IsEditing()) {
        m_RobotConfig->CancelEdit();
    }
}