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
    m_LiveStreamManager     = std::make_unique<LiveStreamManager>();
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
        RestoreRobotStatusActive();
    }
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
        if (s_LastSelectedId != m_selected_id) {
            if (m_selected_id == 2)
                m_NodeGraphManager->RequestNavigate();
            SyncActiveItems(m_selected_id);
            s_LastSelectedId = m_selected_id;
        }

        // 统一策略：每帧检测选中项变化 → 重新同步（处理跨组件依赖）
        int curComponentIdx = m_RobotComponentManager->GetSelectedIndex();
        int curGamepadIdx   = m_GamepadMapperManager->GetSelectedIndex();
        int curNodeGraphIdx = m_NodeGraphManager->GetSelectedIndex();
        int curLiveStreamIdx = -1;
        for (int i = 0; i < m_LiveStreamManager->GetItemCount(); ++i)
            if (m_LiveStreamManager->IsItemSelected(i)) { curLiveStreamIdx = i; break; }
        int curCommIdx = -1;
        for (int i = 0; i < m_RobotCommManager->GetItemCount(); ++i)
            if (m_RobotCommManager->IsItemSelected(i)) { curCommIdx = i; break; }

        if (curComponentIdx != m_LastComponentIdx ||
            curGamepadIdx   != m_LastGamepadIndex ||
            curNodeGraphIdx != m_LastNodeGraphIdx ||
            curLiveStreamIdx != m_LastLiveStreamIdx ||
            curCommIdx      != m_LastCommIdx)
        {
            SyncActiveItems(m_selected_id);
            m_LastComponentIdx  = curComponentIdx;
            m_LastGamepadIndex  = curGamepadIdx;
            m_LastNodeGraphIdx  = curNodeGraphIdx;
            m_LastLiveStreamIdx  = curLiveStreamIdx;
            m_LastCommIdx        = curCommIdx;
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
// 保存/恢复 — BASE 仅取自 Status 的 LiveStream + NodeGraph 选择
// ============================================================================
void RobotSettingPanel::SaveRobotStatusActive()
{
    if (!m_RobotStatus) return;
    m_SavedLiveStreamIdx  = m_RobotStatus->GetActiveLiveStreamIdx();
    m_SavedNodeGraphIdx   = m_RobotStatus->GetActiveNodeGraphIdx();
    WL_INFO_TAG("CONFIG", "Saved BASE: ls={}, ng={}", m_SavedLiveStreamIdx, m_SavedNodeGraphIdx);
}

void RobotSettingPanel::RestoreRobotStatusActive()
{
    if (!m_RobotStatus) return;
    // 恢复 Status 中保存的 LiveStream 和 NodeGraph 选择
    m_RobotStatus->SetActiveLiveStreamIdx(m_SavedLiveStreamIdx);
    m_RobotStatus->SetActiveNodeGraphIdx(m_SavedNodeGraphIdx);
    // 确保 NodeGraphManager 选中项指向保存的 NodeGraph（用于后续推导）
    m_NodeGraphManager->SetSelectedIndex(m_SavedNodeGraphIdx);
    // 从保存的 NodeGraph 重新推导 Component, Gamepad, Comm（由 RobotStatus 统一处理）
    m_RobotStatus->DeriveActiveFromNodeGraph();
    m_RobotStatus->RequestNodeGraphSync();
    WL_INFO_TAG("CONFIG", "Restored BASE: ls={}, ng={}", m_SavedLiveStreamIdx, m_SavedNodeGraphIdx);
}

// ============================================================================
// 统一策略：SyncActiveItems
//
// BASE（来自 Status 窗口的选择）:
//   LiveStream  = m_SavedLiveStreamIdx
//   NodeGraph   = m_SavedNodeGraphIdx
//   Component/Gamepad/Comm = 由 RobotStatus::DeriveActiveFromNodeGraph() 推导
//
// OVERRIDE（当前标签页）:
//   标签页组件 Active = 对应 Manager 当前选中项
//   RobotComm/NodeGraph/GamepadMapper 标签页还需跨组件同步 RobotComponent
// ============================================================================
void RobotSettingPanel::SyncActiveItems(int tabId)
{
    if (!m_RobotStatus) return;

    // ======== BASE: 恢复 Status 选择 + 从 NodeGraph 推导 ========
    m_RobotStatus->SetActiveLiveStreamIdx(m_SavedLiveStreamIdx);
    m_RobotStatus->SetActiveNodeGraphIdx(m_SavedNodeGraphIdx);
    m_RobotStatus->DeriveActiveFromNodeGraph();

    // ======== OVERRIDE: 当前标签页 Active = Manager 选中项 ========
    switch (tabId)
    {
    case 0: // Component
        {
            auto* comp = m_RobotComponentManager->GetSelectedComponent();
            if (comp)
                m_RobotStatus->SetActiveMode(*comp);
        }
        break;

    case 1: // GamepadMapper
        {
            auto* gm = m_GamepadMapperManager->GetSelectedMapper();
            if (gm) {
                m_RobotStatus->SetActiveGamepad(gm);
            }
        }
        break;

    case 2: // NodeGraph
        {
            // 已在 BASE 中从 NodeGraph 推导，但需确保用最新的选中图
            m_RobotStatus->SetActiveNodeGraphIdx(m_NodeGraphManager->GetSelectedIndex());
            m_RobotStatus->DeriveActiveFromNodeGraph();
        }
        break;

    case 3: // LiveStream
        {
            for (int i = 0; i < m_LiveStreamManager->GetItemCount(); ++i)
                if (m_LiveStreamManager->IsItemSelected(i)) {
                    m_RobotStatus->SetActiveLiveStreamIdx(i);
                    break;
                }
        }
        break;

    case 4: // RobotComm
        {
            for (int i = 0; i < m_RobotCommManager->GetItemCount(); ++i)
                if (m_RobotCommManager->IsItemSelected(i)) {
                    m_RobotStatus->SetActiveCommIndices({i});
                    break;
                }
            // 跨组件：RobotComponent Active = 当前选中 comm 节点自己存的 component 选择
            auto* node = m_RobotCommManager->GetSelectedNode();
            if (node) {
                auto& comps = m_RobotComponentManager->GetComponents();
                int compIdx = node->component.active_component_idx;
                if (compIdx >= 0 && compIdx < (int)comps.size())
                    m_RobotStatus->SetActiveMode(comps[compIdx]);
            }
        }
        break;
    }
}
