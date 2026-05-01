#include "OptionPanel.h"

OptionPanel::OptionPanel()
{
    m_GamepadMapper = std::make_unique<GamepadMapper>();
    m_ImGuiStyleManager = std::make_unique<ImGuiStyleManager>();
    m_RobotConfig = std::make_unique<Robot_Config>();
}

OptionPanel::~OptionPanel()
{
}

void OptionPanel::DrawOptionPanel()
{
    float footerHeight = ImGui::GetFrameHeightWithSpacing() + 5.0f;
    float availableHeight = ImGui::GetContentRegionAvail().y - footerHeight;

    if (ImGui::BeginTable("MainLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Sidebar", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (ImGui::BeginChild("SideBarChild", ImVec2(0, availableHeight), true))
        {
            for (int i = 0; i < 3; i++) {
                ImGui::PushID(i);
                if (ImGui::Selectable(items[i], selected_id == i, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 30))) {
                    selected_id = i;
                }
                ImGui::PopID();
            }
        }

        ImGui::EndChild();

        ImGui::TableSetColumnIndex(1);
        if (ImGui::BeginChild("DetailsChild", ImVec2(0, availableHeight), false))
        {
            ImGui::Indent(10.0f);
            ImGui::Spacing();
            if (selected_id == 0) {
                m_RobotConfig->DrawRobotConfigPanel();
            }
            else  if (selected_id == 1) {
                m_GamepadMapper->DrawGamepadMapper();
            }
            else if (selected_id == 2) {
                m_ImGuiStyleManager->DrawStylePanel();
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
    // 应用当前选中的主题
    m_ImGuiStyleManager->ApplyImGuiStyle(m_ImGuiStyleManager->GetTheme(), m_ImGuiStyleManager->GetInvert(), m_ImGuiStyleManager->GetAlpha());

    // 应用手柄的设置
    m_GamepadMapper->ApplyMappings();

    // 应用机器人参数
    if (m_RobotConfig && m_RobotConfigChanged) {
        m_RobotConfigChanged = false;
        configChanged = m_RobotConfig->ApplyConfig();
    }

    return configChanged;
}

void OptionPanel::Revert()
{
    if (m_GamepadMapper) m_GamepadMapper->RevertMappings();
    if (m_RobotConfig) {
        m_RobotConfig->RevertConfig();
        m_RobotConfigChanged = false;
    }
    if (m_ImGuiStyleManager) {
        m_ImGuiStyleManager->RevertStyle();
    }
}
