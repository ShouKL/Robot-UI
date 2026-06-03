#define IMGUI_DEFINE_MATH_OPERATORS
#include "NodeGraphManager.h"
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
    // new item starts empty — no need to snapshot
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
    if (index < 0) return;
    DeleteByIndex(index);
}

void NodeGraphManager::DeleteByIndex(int index)
{
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
    }
}

void NodeGraphManager::LoadItemToCurrent()
{
    if (m_SelectedIndex >= 0 && m_SelectedIndex < (int)m_Items.size()) {
        m_SelectedGraph = m_Items[m_SelectedIndex].graph.get();
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
    m_EditorCtx = ed::CreateEditor(&cfg);
    ed::SetCurrentEditor(m_EditorCtx);
    AddItem();
    m_Items[0].isSelected = true;
    m_SelectedGraph = m_Items[0].graph.get();
    ed::SetCurrentEditor(nullptr);  // clear global context — DrawContent sets it on open
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
        snap.push_back(std::move(clone));
    }
    return snap;
}

void NodeGraphManager::LoadItems(const std::vector<GraphItem>& items)
{
    int oldSelectedIdx = m_SelectedIndex;
    m_Items.clear();
    for (const auto& src : items) {
        GraphItem item;
        item.id = src.id;
        item.isSelected = src.isSelected;
        strncpy_s(item.name, src.name, sizeof(item.name) - 1);
        item.graph = src.graph->Clone();
        item.editorYaml = src.editorYaml;
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
}

// ============================================================================
// GetGraphYaml / LoadGraphYaml — delegates to m_SelectedGraph (Manager sets ed:: context)
// ============================================================================
std::string NodeGraphManager::GetGraphYaml() const
{
    ed::SetCurrentEditor(m_EditorCtx);
    return m_SelectedGraph->GetGraphYaml();
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
    m_SelectedGraph->Draw(m_EditorCtx);
}

