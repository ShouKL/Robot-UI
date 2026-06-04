#include "RobotSettingPanel.h"
#include "RobotStatus.h"
#include "Walnut/Core/Log.h"
#include "imgui.h"
#include <imgui_node_editor.h>
namespace ed = ax::NodeEditor;

RobotSettingPanel::RobotSettingPanel()
{
    m_RobotComponentManager = std::make_unique<RobotComponentManager>();
    m_GamepadMapperManager  = std::make_unique<GamepadMapperManager>();
    m_NodeGraphManager      = std::make_unique<NodeGraphManager>();
    m_LiveStreamManager  = std::make_unique<LiveStreamManager>();
    m_RobotCommManager   = std::make_unique<RobotCommManager>();
    WL_INFO_TAG("APP", "RobotSettingPanel created");
}

void RobotSettingPanel::BeginEdit()
{
    if (IsEditing()) return;
    EditDraftBase::BeginEdit();
    TakeSnapshots();
    SaveRobotStatusActive();
    // 编辑期间：evaluator 实时跟随 Manager 选中图
    if (m_RobotStatus) m_RobotStatus->EnableLiveSync(true);
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
    // 关闭 live sync，回到 RobotStatus 自己的 active 图
    if (m_RobotStatus) {
        m_RobotStatus->EnableLiveSync(false);
        m_RobotStatus->SyncActiveNodeGraph();
        auto* gm = m_GamepadMapperManager->GetSelectedMapper();
        if (gm) m_RobotStatus->SetActiveGamepad(gm);
    }
    m_SavedActiveMode    = nullptr;
    m_SavedActiveGamepad = nullptr;
    EditDraftBase::ApplyEdit();
}

void RobotSettingPanel::CancelEdit()
{
    WL_INFO_TAG("CONFIG", "Reverting RobotSetting...");
    // 关闭 live sync，恢复已保存状态
    if (m_RobotStatus) m_RobotStatus->EnableLiveSync(false);
    if (m_RobotComponentManager && !m_ComponentSnapshot.empty()) {
        m_RobotComponentManager->LoadItems(m_ComponentSnapshot);
        m_ComponentSnapshot.clear();
    }
    if (m_GamepadMapperManager && !m_GamepadSnapshot.empty()) {
        m_GamepadMapperManager->LoadItems(m_GamepadSnapshot);
        m_GamepadSnapshot.clear();
    }
    if (m_LiveStreamManager && !m_StreamSnapshot.empty()) {
        m_LiveStreamManager->LoadItems(m_StreamSnapshot);
        m_StreamSnapshot.clear();
    }
    if (m_RobotCommManager && !m_CommSnapshot.empty()) {
        m_RobotCommManager->LoadItems(m_CommSnapshot);
        m_CommSnapshot.clear();
    }
    if (m_NodeGraphManager && !m_NodeGraphSnapshot.empty()) {
        m_NodeGraphManager->LoadItems(m_NodeGraphSnapshot);
        m_NodeGraphSnapshot.clear();
    }
    RestoreRobotStatusActive();
    EditDraftBase::CancelEdit();
}

void RobotSettingPanel::TakeSnapshots()
{
    if (m_RobotComponentManager)
        m_ComponentSnapshot = m_RobotComponentManager->GetAllItems();
    if (m_GamepadMapperManager)
        m_GamepadSnapshot = m_GamepadMapperManager->GetAllItems();
    if (m_LiveStreamManager)
        m_StreamSnapshot = m_LiveStreamManager->GetAllItems();
    if (m_RobotCommManager)
        m_CommSnapshot = m_RobotCommManager->GetAllItems();
    if (m_NodeGraphManager)
        m_NodeGraphSnapshot = m_NodeGraphManager->GetAllItems();
}

void RobotSettingPanel::Draw(bool* p_open)
{
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(displaySize.x * 0.85f, displaySize.y * 0.8f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(500, 400), ImVec2(displaySize.x, displaySize.y));

    if (!ImGui::Begin("Robot Setting", p_open, 0))
    {
        ImGui::End();
        // 点击 X 关闭时：关闭 live sync，回到 RobotStatus 自己的 active
        if (IsEditing()) {
            ApplyEdit();
        }
        return;
    }

    // 编辑期间每帧：若在 NodeGraph 标签页，实时同步 evaluator 到 Manager 选中图
    if (m_RobotStatus)
        m_RobotStatus->SyncFromManagerIfLive();

    float footerHeight = ImGui::GetFrameHeightWithSpacing() + 5.0f;
    float availableHeight = ImGui::GetContentRegionAvail().y - footerHeight;

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

    if (ImGui::BeginChild("RSSSubBar", ImVec2(150, availableHeight), true))
    {
        ManagerBase* sideMgrs[] = { m_RobotComponentManager.get(), m_GamepadMapperManager.get(), m_NodeGraphManager.get(), m_LiveStreamManager.get(), m_RobotCommManager.get() };
        if (m_selected_id >= 0 && m_selected_id < 5)
            sideMgrs[m_selected_id]->DrawItemList(150.0f);
    }
    ImGui::EndChild();
    ImGui::SameLine();

    {
        static int s_LastSelectedId = -1;
        if (s_LastSelectedId != 2 && m_selected_id == 2) {
            m_NodeGraphManager->RequestNavigate();
            // 调试：切到 NodeGraph 时，同步全局 active gamepad
            auto* gm = m_GamepadMapperManager->GetSelectedMapper();
            if (m_RobotStatus && gm) m_RobotStatus->SetActiveGamepad(gm);
        }
        s_LastSelectedId = m_selected_id;

        // 调试：GamepadMapper 中切换 item 时立即同步 active gamepad
        if (m_selected_id == 1) {
            int curIdx = m_GamepadMapperManager->GetSelectedIndex();
            if (curIdx != m_LastGamepadIndex) {
                m_LastGamepadIndex = curIdx;
                auto* gm = m_GamepadMapperManager->GetSelectedMapper();
                if (m_RobotStatus && gm) m_RobotStatus->SetActiveGamepad(gm);
            }
        }

        if (ImGui::BeginChild("RSSDetail", ImVec2(0, availableHeight), false, ImGuiWindowFlags_NoScrollbar))
        {
            ManagerBase* contentMgrs[] = { m_RobotComponentManager.get(), m_GamepadMapperManager.get(), m_NodeGraphManager.get(), m_LiveStreamManager.get(), m_RobotCommManager.get() };
            if (m_selected_id >= 0 && m_selected_id < 5)
                contentMgrs[m_selected_id]->DrawContent();
        }
        ImGui::EndChild();
    }

    float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

    if (ImGui::Button("Apply", ImVec2(buttonWidth, 0)))
        ApplyEdit();

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
    {
        CancelEdit();
        // Cancel 后恢复 RobotStatus active 状态已在 CancelEdit 中调用
    }

    ImGui::End();
}

// ============================================================================
// 保存/恢复 RobotStatus 的 active 状态（调试用）
// ============================================================================
void RobotSettingPanel::SaveRobotStatusActive()
{
    if (!m_RobotStatus) return;
    m_SavedActiveMode     = m_RobotStatus->GetActiveModePtr();
    m_SavedActiveGamepad  = m_RobotStatus->GetActiveGamepadPtr();
    m_SavedLiveStreamIdx  = m_RobotStatus->GetActiveLiveStreamIdx();
    m_SavedNodeGraphIdx   = m_RobotStatus->GetActiveNodeGraphIdx();
    m_SavedCommIdx        = m_RobotStatus->GetActiveCommIdx();
    WL_INFO_TAG("CONFIG", "RobotStatus active state saved (mode={}, gamepad={}, ls={}, ng={}, comm={})",
        m_SavedActiveMode ? m_SavedActiveMode->name : "null",
        m_SavedActiveGamepad ? m_SavedActiveGamepad->name : "null",
        m_SavedLiveStreamIdx, m_SavedNodeGraphIdx, m_SavedCommIdx);
}

void RobotSettingPanel::RestoreRobotStatusActive()
{
    if (!m_RobotStatus) return;
    if (m_SavedActiveMode)
        m_RobotStatus->SetActiveMode(m_SavedActiveMode);
    if (m_SavedActiveGamepad)
        m_RobotStatus->SetActiveGamepad(m_SavedActiveGamepad);
    m_RobotStatus->SetActiveLiveStreamIdx(m_SavedLiveStreamIdx);
    m_RobotStatus->SetActiveNodeGraphIdx(m_SavedNodeGraphIdx);
    m_RobotStatus->SetActiveCommIdx(m_SavedCommIdx);
    m_RobotStatus->RequestNodeGraphSync();  // 下一帧 DrawWindow 时触发同步
    WL_INFO_TAG("CONFIG", "RobotStatus active state restored (mode={}, gamepad={}, ls={}, ng={}, comm={})",
        m_SavedActiveMode ? m_SavedActiveMode->name : "null",
        m_SavedActiveGamepad ? m_SavedActiveGamepad->name : "null",
        m_SavedLiveStreamIdx, m_SavedNodeGraphIdx, m_SavedCommIdx);
    m_SavedActiveMode    = nullptr;
    m_SavedActiveGamepad = nullptr;
}
