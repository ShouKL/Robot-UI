#pragma once

#include <algorithm>
#include <vector>
#include <cstdio>
#include "imgui.h"

class ManagerBase {
public:
    virtual ~ManagerBase() = default;

    virtual void DrawItemList(float width) {}
    virtual void DrawContent() {}

    virtual void AddItem() = 0;
    virtual void RemoveItem(int id) = 0;

    void Open()  { m_Open = true; }
    void Close() { m_Open = false; }
    bool IsOpen() const { return m_Open; }
    bool* GetOpenPtr() { return &m_Open; }

protected:
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
