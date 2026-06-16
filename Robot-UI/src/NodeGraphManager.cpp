#define IMGUI_DEFINE_MATH_OPERATORS
#include "NodeGraphManager.h"
#include "RobotComponentManager.h"
#include "GamepadMapperManager.h"
#include "RobotCommManager.h"
#include "Walnut/Core/Log.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_node_editor.h>
#include <yaml-cpp/yaml.h>
#include <algorithm>

namespace ed = ax::NodeEditor;

// ============================================================================
// ManagerBase: AddItem / RemoveItem / Select
// ============================================================================
void NodeGraphManager::AddItem()
{
    GraphItem item;
    item.id = NextId();
    snprintf(item.name, sizeof(item.name), "Item_%d", item.id);
    item.graph = std::make_unique<NodeGraph>();
    if (m_RobotMgr)     item.graph->SetRobotComponentManager(m_RobotMgr);
    if (m_GamepadMgr)   item.graph->SetGamepadMapperManager(m_GamepadMgr);
    if (m_RobotCommMgr) item.graph->SetRobotCommManager(m_RobotCommMgr);
    m_Items.push_back(std::move(item));
    if (m_Items.size() == 1) {
        m_SelectedIndex = 0;
        m_Items[0].isSelected = true;
        m_SelectedGraph = m_Items[0].graph.get();
    }
}

void NodeGraphManager::RemoveItem(int id)
{
    int index = FindNodeIndex(m_Items, id);
    if (index < 0 || index >= (int)m_Items.size()) return;
    if (m_Items.size() <= 1) return;
    m_Items.erase(m_Items.begin() + index);
    if (m_SelectedIndex >= (int)m_Items.size())
        m_SelectedIndex = (int)m_Items.size() - 1;
    if (!m_Items.empty()) {
        m_Items[m_SelectedIndex].isSelected = true;
        m_SelectedGraph = m_Items[m_SelectedIndex].graph.get();
    }
}

void NodeGraphManager::SetSelectedIndex(int idx)
{
    if (idx >= 0 && idx < (int)m_Items.size()) {
        SaveCurrentToItem();
        for (auto& it : m_Items) it.isSelected = false;
        m_SelectedIndex = idx;
        m_Items[idx].isSelected = true;
        LoadItemToCurrent();
        m_SelectedGraph->RequestNavigate();
    }
}

void NodeGraphManager::RenameItem(int id, const char* newName)
{
    for (auto& item : m_Items)
        if (item.id == id) { strncpy_s(item.name, newName, sizeof(item.name) - 1); break; }
}

void NodeGraphManager::SaveCurrentToItem()
{
    if (m_SelectedIndex >= 0 && m_SelectedIndex < (int)m_Items.size()) {
        auto& item = m_Items[m_SelectedIndex];
        item.editorYaml = GetGraphYaml();
        item.graph->LoadGraphData(item.editorYaml);
        item.comm_index = m_SelectedGraph->GetCommIndex();
    }
}

void NodeGraphManager::LoadItemToCurrent()
{
    if (m_SelectedIndex >= 0 && m_SelectedIndex < (int)m_Items.size()) {
        auto& item = m_Items[m_SelectedIndex];
        m_SelectedGraph = item.graph.get();
        m_SelectedGraph->SetCommIndex(item.comm_index);
        // 重新注入依赖到新选中的 graph
        if (m_RobotMgr)     m_SelectedGraph->SetRobotComponentManager(m_RobotMgr);
        if (m_GamepadMgr)   m_SelectedGraph->SetGamepadMapperManager(m_GamepadMgr);
        if (m_RobotCommMgr) m_SelectedGraph->SetRobotCommManager(m_RobotCommMgr);
        // mode name 从 YAML 中恢复（GetGraphYaml 已序列化）
        LoadGraphYaml(m_Items[m_SelectedIndex].editorYaml);
    }
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
NodeGraphManager::NodeGraphManager()
{
    ed::Config cfg;
    cfg.SettingsFile = nullptr;
    cfg.DragButtonIndex = ImGuiMouseButton_Right;     // 右键拖拽节点
    cfg.SelectButtonIndex = ImGuiMouseButton_Left;     // 左键：点引脚连线 / 选节点
    cfg.NavigateButtonIndex = ImGuiMouseButton_Middle;
    m_EditorCtx = ed::CreateEditor(&cfg);
    ed::SetCurrentEditor(m_EditorCtx);
    AddItem();
    m_Items[0].isSelected = true;
    m_SelectedGraph = m_Items[0].graph.get();
    ed::SetCurrentEditor(nullptr);  // clear global context — DrawContent sets it on open
}

void NodeGraphManager::SetRobotCommManager(RobotCommManager* comm)
{
    m_RobotCommMgr = comm;
    if (m_SelectedGraph) m_SelectedGraph->SetRobotCommManager(comm);
}

NodeGraphManager::~NodeGraphManager()
{
    if (m_EditorCtx)
    {
        ed::SetCurrentEditor(nullptr);
        ed::DestroyEditor(m_EditorCtx);
        m_EditorCtx = nullptr;
    }
}

void NodeGraphManager::ApplyChanges()
{
    m_SelectedGraph->SaveGraphToMap();
    m_SelectedGraph->SetModified(false);
}

void NodeGraphManager::SetRobotComponentManager(RobotComponentManager* c)
{
    m_RobotMgr = c;
    if (m_SelectedGraph) m_SelectedGraph->SetRobotComponentManager(c);
}

void NodeGraphManager::SetGamepadMapperManager(GamepadMapperManager* g)
{
    m_GamepadMgr = g;
    if (m_SelectedGraph) m_SelectedGraph->SetGamepadMapperManager(g);
}

std::vector<GraphItem> NodeGraphManager::GetAllItems() const
{
    // Save current changes to the active item before snapshotting
    if (m_SelectedIndex >= 0 && m_SelectedIndex < (int)m_Items.size()) {
        auto& item = const_cast<NodeGraphManager*>(this)->m_Items[m_SelectedIndex];
        item.editorYaml = const_cast<NodeGraphManager*>(this)->GetGraphYaml();
        item.graph->LoadGraphData(item.editorYaml);
    }
    // Deep-clone all items
    std::vector<GraphItem> snap;
    for (const auto& it : m_Items) {
        GraphItem clone;
        clone.id = it.id;
        clone.isSelected = it.isSelected;
        strncpy_s(clone.name, it.name, sizeof(clone.name) - 1);
        clone.graph = it.graph->Clone();
        clone.editorYaml = it.editorYaml;
        clone.comm_index = it.comm_index;
        snap.push_back(std::move(clone));
    }
    return snap;
}

void NodeGraphManager::LoadItems(const std::vector<GraphItem>& items)
{
    auto kvSnapshot = m_SelectedGraph ? m_SelectedGraph->GetKeyValuesSnapshot() : std::map<std::string, float>{};
    int oldSelectedIdx = m_SelectedIndex;
    m_Items.clear();
    for (const auto& src : items) {
        GraphItem item;
        item.id = src.id;
        item.isSelected = src.isSelected;
        strncpy_s(item.name, src.name, sizeof(item.name) - 1);
        if (src.graph)
            item.graph = src.graph->Clone();
        else {
            item.graph = std::make_unique<NodeGraph>();
            if (m_RobotMgr)  item.graph->SetRobotComponentManager(m_RobotMgr);
            if (m_GamepadMgr) item.graph->SetGamepadMapperManager(m_GamepadMgr);
        }
        item.editorYaml = src.editorYaml;
        item.comm_index = src.comm_index;
        m_Items.push_back(std::move(item));
    }
    if (m_Items.empty()) {
        AddItem();
        m_Items[0].isSelected = true;
        m_SelectedIndex = 0;
    } else {
        m_SelectedIndex = (oldSelectedIdx >= 0 && oldSelectedIdx < (int)m_Items.size()) ? oldSelectedIdx : 0;
        for (auto& it : m_Items) it.isSelected = false;
        m_Items[m_SelectedIndex].isSelected = true;
    }
    LoadItemToCurrent();
    // 预填充 key values，避免侧栏闪空
    if (m_SelectedGraph) m_SelectedGraph->SetKeyValues(kvSnapshot);
}

// ============================================================================
// GetGraphYaml / LoadGraphYaml — delegates to m_SelectedGraph (Manager sets ed:: context)
// ============================================================================
std::string NodeGraphManager::GetGraphYaml() const
{
    ed::SetCurrentEditor(m_EditorCtx);
    return m_SelectedGraph->GetGraphYaml();
}

std::string NodeGraphManager::GetGraphYamlForIndex(int idx)
{
    if (idx < 0 || idx >= (int)m_Items.size()) return {};
    // 如果是当前选中项，先保存未提交的编辑到 item
    if (idx == m_SelectedIndex) {
        const_cast<NodeGraphManager*>(this)->SaveCurrentToItem();
    }
    return m_Items[idx].graph->GetGraphDataYaml();
}

bool NodeGraphManager::LoadGraphYaml(const std::string& yamlStr)
{
    if (yamlStr.empty()) return false;
    ed::SetCurrentEditor(m_EditorCtx);
    return m_SelectedGraph->LoadGraphYaml(yamlStr);
}
// ============================================================================
// DrawContent — delegates to NodeGraph::Draw
// ============================================================================
void NodeGraphManager::DrawContent()
{
    if (m_SelectedGraph)
        m_SelectedGraph->Draw(m_EditorCtx);
}

