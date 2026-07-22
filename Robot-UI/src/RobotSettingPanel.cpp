#include "RobotSettingPanel.h"
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
    // 手动同步一次当前图到 evaluator，然后恢复 RobotStatus 的 active 选择
    // 同时触发连接同步（仅 Apply 时同步，Play 不再触发）
    if (m_RobotStatus) {
        m_RobotStatus->SyncFromManagerSelected();
        RestoreRobotStatusActive();              // 先 Derive 更新 m_ActiveCommIndices
        m_RobotStatus->SyncConnectionsFromGraph(); // 再根据 indices 重建连接池
    }
    EditDraftBase::ApplyEdit();
}

void RobotSettingPanel::CancelEdit()
{
    WL_INFO_TAG("CONFIG", "Reverting RobotSetting...");
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
        // Save current editor state to item BEFORE clearing (prevents dangling pointer)
        m_NodeGraphManager->SaveCurrentToItem();
        m_NodeGraphManager->LoadItems(m_NodeGraphSnapshot);
        m_NodeGraphSnapshot.clear();
        // Ensure the restored graph is valid for rendering
        if (m_NodeGraphManager->GetSelectedGraph())
            m_NodeGraphManager->GetSelectedGraph()->RequestNavigate();
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
    // 暖机模式：将窗口设为全透明，正常渲染内容（上传 GPU 纹理），但用户不可见
    if (m_WarmupMode)
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f);

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
        if (m_WarmupMode)
            ImGui::PopStyleVar();
        return;
    }

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

    if (m_WarmupMode)
        ImGui::PopStyleVar();
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
// 统一策略：SyncActiveItems（不再修改 RobotStatus 的 active）
//
// 由于 Connect ↔ RobotSetting 已完全互斥（连接时关闭面板，打开面板时断开），
// Setting 面板的选中项变更不应影响 RobotStatus 的运行时 active 状态。
// RobotStatus 通过自己的 UI combo 独立管理 active 选择。
// 跨组件同步（如 Comm→Component 的 active_component_idx）在各 Manager 的
// DrawContent 中通过直接传递引用实现，无需通过 RobotStatus 中转。
// ============================================================================
void RobotSettingPanel::SyncActiveItems(int tabId)
{
    (void)tabId;
    // 不再修改 RobotStatus 的 active 状态
    // 原逻辑（SetActiveMode / SetActiveGamepad / DeriveActiveFromNodeGraph 等）已移除
}
