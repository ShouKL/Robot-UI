#include "RobotSettingPanel.h"
#include "Walnut/Core/Log.h"
#include "imgui.h"

RobotSettingPanel::RobotSettingPanel()
{
    m_RobotComponentManager = std::make_unique<RobotComponentManager>();
    m_GamepadMapperManager  = std::make_unique<GamepadMapperManager>();
    m_NodeGraphManager      = std::make_unique<NodeGraphManager>();
    m_LiveStreamManager  = std::make_unique<LiveStreamManager>();
    m_RobotCommManager   = std::make_unique<RobotCommManager>();
    WL_INFO_TAG("APP", "RobotSettingPanel created");
}

void RobotSettingPanel::Open()
{
    m_Open = true;
    if (!IsEditing())
        BeginEdit();
}

void RobotSettingPanel::BeginEdit()
{
    if (IsEditing()) return;
    EditDraftBase::BeginEdit();
    TakeSnapshots();
    WL_INFO_TAG("CONFIG", "RobotSetting editing started");
}

void RobotSettingPanel::ApplyEdit()
{
    WL_INFO_TAG("CONFIG", "Applying RobotSetting...");
    m_ComponentSnapshot.clear();
    m_GamepadSnapshot.clear();
    m_StreamSnapshot.clear();
    m_CommSnapshot.clear();
    if (m_NodeGraphManager) {
        m_NodeGraphManager->ApplyChanges();
        m_NodeGraphSnapshot.clear();
    }
    EditDraftBase::ApplyEdit();
}

void RobotSettingPanel::CancelEdit()
{
    WL_INFO_TAG("CONFIG", "Reverting RobotSetting...");
    if (m_RobotComponentManager && !m_ComponentSnapshot.empty()) {
        m_RobotComponentManager->RestoreComponents(m_ComponentSnapshot);
        m_ComponentSnapshot.clear();
    }
    if (m_GamepadMapperManager && !m_GamepadSnapshot.empty()) {
        m_GamepadMapperManager->RestoreMappers(m_GamepadSnapshot);
        m_GamepadSnapshot.clear();
    }
    if (m_LiveStreamManager && !m_StreamSnapshot.empty()) {
        m_LiveStreamManager->LoadAllConfigs(m_StreamSnapshot);
        m_StreamSnapshot.clear();
    }
    if (m_RobotCommManager && !m_CommSnapshot.empty()) {
        m_RobotCommManager->LoadConfigs(m_CommSnapshot, m_CommActiveIdSnapshot);
        m_CommSnapshot.clear();
    }
    if (m_NodeGraphManager && !m_NodeGraphSnapshot.empty()) {
        m_NodeGraphManager->CancelChanges();
        m_NodeGraphSnapshot.clear();
    }
    EditDraftBase::CancelEdit();
}

void RobotSettingPanel::TakeSnapshots()
{
    if (m_RobotComponentManager)
        m_ComponentSnapshot = m_RobotComponentManager->GetAllComponents();
    if (m_GamepadMapperManager)
        m_GamepadSnapshot = m_GamepadMapperManager->GetAllMappers();
    if (m_LiveStreamManager)
        m_StreamSnapshot = m_LiveStreamManager->GetAllStreamConfigs();
    if (m_RobotCommManager) {
        m_CommSnapshot = m_RobotCommManager->GetAllConfigs();
        m_CommActiveIdSnapshot = m_RobotCommManager->GetActiveId();
    }
    if (m_NodeGraphManager) {
        m_NodeGraphManager->OnOpen();
        m_NodeGraphSnapshot = m_NodeGraphManager->GetGraphYaml();
    }
}

void RobotSettingPanel::Draw()
{
    if (!m_Open) return;

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(displaySize.x * 0.85f, displaySize.y * 0.8f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(500, 400), ImVec2(displaySize.x, displaySize.y));

    // NOTE: Do NOT use ImGuiWindowFlags_AlwaysVerticalScrollbar here.
    // It forces the window to split draw channels for the scrollbar, and
    // imgui-node-editor's internal ImDrawList_SwapSplitter corrupts that
    // state, leaking sentinel callbacks into the final command buffer
    // and triggering the "This draw callback should never be called" assertion.
    if (!ImGui::Begin("Robot Setting", &m_Open, 0))
    {
        ImGui::End();
        return;
    }

    float footerHeight = ImGui::GetFrameHeightWithSpacing() + 5.0f;
    float availableHeight = ImGui::GetContentRegionAvail().y - footerHeight;

    // Use manual 3-panel layout (BeginChild + SameLine) for ALL tabs.
    // BeginTable would cause a one-frame flash when switching to/from NodeGraph
    // because imgui-node-editor's draw-channel splitters conflict with table
    // channel management.

    // ---- 第1列：大类选择 ----
    if (ImGui::BeginChild("RSSSideBar", ImVec2(150, availableHeight), true))
    {
        if (!IsEditing())
            BeginEdit();

        const char* items[] = { "Component", "GamepadMapper", "NodeGraph", "Live Streamer", "Robot Comm" };
        for (int i = 0; i < IM_ARRAYSIZE(items); i++) {
            ImGui::PushID(i);
            if (ImGui::Selectable(items[i], m_selected_id == i, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 30)))
                m_selected_id = i;
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ---- 第2列：子侧边栏 / AddItem 列表 ----
    if (ImGui::BeginChild("RSSSubBar", ImVec2(150, availableHeight), true))
    {
        ManagerBase* sideMgrs[] = { m_RobotComponentManager.get(), m_GamepadMapperManager.get(), m_NodeGraphManager.get(), m_LiveStreamManager.get(), m_RobotCommManager.get() };
        if (m_selected_id >= 0 && m_selected_id < 5)
            sideMgrs[m_selected_id]->DrawItemList(150.0f);
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ---- 第3列：内容区 ----
    // imgui-node-editor manipulates draw channels on the current window's
    // draw list; nesting it inside BeginChild causes sentinel callbacks to
    // leak to the parent window when EndChild merges channels, resulting in
    // a full-screen flash. Render NodeGraph directly without a child window.
    if (m_selected_id == 2)
    {
        ImGui::Indent(10.0f);
        ImGui::Spacing();
        m_NodeGraphManager->DrawContent();
        ImGui::Unindent(10.0f);
    }
    else if (ImGui::BeginChild("RSSDetail", ImVec2(0, availableHeight), false))
    {
        ImGui::Indent(10.0f);
        ImGui::Spacing();

        ManagerBase* contentMgrs[] = { m_RobotComponentManager.get(), m_GamepadMapperManager.get(), m_NodeGraphManager.get(), m_LiveStreamManager.get(), m_RobotCommManager.get() };
        if (m_selected_id >= 0 && m_selected_id < 5)
            contentMgrs[m_selected_id]->DrawContent();

        ImGui::Unindent(10.0f);
    }
    if (m_selected_id != 2)
        ImGui::EndChild();

    float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

    if (ImGui::Button("Apply", ImVec2(buttonWidth, 0)))
        ApplyEdit();

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
    {
        CancelEdit();
        m_Open = false;
    }

    ImGui::End();
}
