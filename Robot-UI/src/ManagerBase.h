#pragma once

#include <algorithm>
#include <vector>
#include <cstdio>
#include "imgui.h"

class ManagerBase {
public:
    virtual ~ManagerBase() = default;

    virtual void DrawUI(const char* windowName, bool* p_open) = 0;

    virtual void AddItem() = 0;
    virtual void RemoveItem(int id) = 0;

    void Open()  { m_Open = true; }
    void Close() { m_Open = false; }
    bool IsOpen() const { return m_Open; }
    bool* GetOpenPtr() { return &m_Open; }

protected:
    template<typename NodeVec>
    void DrawItemTree(const char* deleteLabel, NodeVec& nodes,
                      const char* (*getName)(const typename NodeVec::value_type&),
                      bool (*getActive)(const typename NodeVec::value_type&))
    {
        int nodeToDelete = -1;

        for (auto& node : nodes) {
            char label[128];
            snprintf(label, sizeof(label), "%s##%d", getName(node), node.id);

            if (ImGui::Selectable(label, node.isSelected)) {
                for (auto& n : nodes) n.isSelected = false;
                node.isSelected = true;
            }

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem(deleteLabel))
                    nodeToDelete = node.id;
                ImGui::EndPopup();
            }

            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30);
            if (getActive(node))
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "ON");
            else
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "OFF");
        }

        if (nodeToDelete != -1) RemoveItem(nodeToDelete);
    }

    template<typename NodeVec>
    int FindNodeIndex(const NodeVec& nodes, int id) const {
        for (int i = 0; i < (int)nodes.size(); ++i)
            if (nodes[i].id == id) return i;
        return -1;
    }

    int   NextId() { return m_NextId++; }
    void  ResetNextId(int start = 1) { m_NextId = start; }
    int   GetNextId() const { return m_NextId; }

    bool m_Open = true;

private:
    int m_NextId = 1;
};
