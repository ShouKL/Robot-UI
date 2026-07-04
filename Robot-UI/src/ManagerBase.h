#pragma once

#include "core.h"

class ManagerBase {
public:
    virtual ~ManagerBase() = default;

    // ========== 必须由子类实现：数据访问 ==========
    virtual int     GetItemCount() const = 0;
    virtual int     GetItemId(int index) const = 0;
    virtual char*   GetItemNameBuf(int index) = 0;
    virtual size_t  GetItemNameBufSize(int index) const { return 64; }

    // ========== 可选覆写：列表行为 ==========
    virtual int  GetSelectedIndex() const { return -1; }
    virtual bool IsItemSelected(int index) const { return index == GetSelectedIndex(); }
    virtual void SelectItem(int index);
    virtual const char* GetDeleteLabel() const { return "Delete Item"; }
    virtual bool CanDeleteItem(int index) const { return GetItemCount() > 1; }
    virtual void DrawItemExtras(int index);

    // ========== 统一列表绘制（子类通常不再需要覆写） ==========
    virtual void DrawItemList(float width);
    virtual void DrawContent() {}

    // ---- Clipboard (Copy/Paste via YAML) ----
    // Override in subclasses to serialize/deserialize the selected item.
    // Return empty string if copy is not supported / nothing selected.
    virtual std::string ClipboardCopySelected() { return {}; }
    virtual void        ClipboardPaste(const std::string& yaml) {}

    virtual void AddItem() = 0;
    virtual void RemoveItem(int id) = 0;
    virtual void RenameItem(int id, const char* newName) = 0;

    void Open()  { m_Open = true; }
    void Close() { m_Open = false; }
    bool IsOpen() const { return m_Open; }
    bool* GetOpenPtr() { return &m_Open; }

    // ---- Internal clipboard (right-click Copy/Paste) ----
    void CopyItemToClipboard(int index);
    void PasteItemFromClipboard();

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

    // ---- Unified inline rename support ----
    // Call from DrawItemList: returns true if Selectable was clicked.
    // Double-click starts inline rename; Enter confirms, Esc cancels.
    bool DrawItemLabel(int id, char* nameBuf, size_t nameBufSize, bool isSelected, float height = 30.0f);

    bool m_Open = true;

    // ---- Internal clipboard ----
    std::string m_ClipboardData;  // YAML snapshot from last "Copy"

private:
    int  m_NextId     = 1;
    int  m_RenamingKey = -1;   // 列表中项的 index（唯一），用于防止重名 id 导致多重命名
    int  m_RenamingItemId = -1; // 重命名项的实际 component id，供 RenameItem 回调
    char m_RenameBuffer[128] = {};
};

// ============================================================================
// DrawItemLabel — unified inline rename widget
// Renders a Selectable (with double-click support) or InputText during rename.
// Returns true when the Selectable is single-clicked (item selection changed).
// ============================================================================
inline bool ManagerBase::DrawItemLabel(int id, char* nameBuf, size_t nameBufSize, bool isSelected, float height)
{
    if (m_RenamingKey == id)
    {
        ImGui::SetNextItemWidth(-1);
        bool confirm = ImGui::InputText("##rename", m_RenameBuffer, sizeof(m_RenameBuffer),
                                        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        bool canceled = ImGui::IsItemDeactivatedAfterEdit() && !ImGui::IsKeyDown(ImGuiKey_Enter)
                        && !ImGui::IsKeyDown(ImGuiKey_KeypadEnter);

        if (confirm || canceled)
        {
            if (confirm && m_RenameBuffer[0] != '\0')
                RenameItem(m_RenamingItemId, m_RenameBuffer);
            m_RenamingKey = -1;
            m_RenamingItemId = -1;
            m_RenameBuffer[0] = '\0';
        }
        return false;
    }

    ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick;
    bool clicked = ImGui::Selectable(nameBuf, isSelected, flags, ImVec2(0, height));

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        m_RenamingKey = id;
        m_RenamingItemId = id;  // 默认：key 和 itemId 相同；DrawItemList 会在调用后覆写 m_RenamingItemId
        strncpy_s(m_RenameBuffer, nameBuf, sizeof(m_RenameBuffer) - 1);
    }

    return clicked;
}

// ============================================================================
// SelectItem / DrawItemExtras — 默认空实现
// ============================================================================
inline void ManagerBase::SelectItem(int index) { (void)index; }
inline void ManagerBase::DrawItemExtras(int index) { (void)index; }

// ============================================================================
// DrawItemList — 统一列表绘制（子类通过虚函数注入差异）
// ============================================================================
inline void ManagerBase::DrawItemList(float width)
{
    (void)width;

    // ---- Keyboard shortcuts: Ctrl+C / Ctrl+V ----
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
    {
        int selIdx = GetSelectedIndex();
        bool suppress = ImGui::IsAnyItemActive(); // don't steal while editing text
        if (!suppress && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C) && selIdx >= 0)
            CopyItemToClipboard(selIdx);
        if (!suppress && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V))
            PasteItemFromClipboard();
    }

    int nodeToDelete = -1;
    int nodeToCopy   = -1;
    for (int i = 0; i < GetItemCount(); ++i) {
        int id = GetItemId(i);
        ImGui::PushID(i);
        if (DrawItemLabel(i, GetItemNameBuf(i), GetItemNameBufSize(i), IsItemSelected(i))) {
            SelectItem(i);
        } else if (m_RenamingKey == i) {
            m_RenamingItemId = id;
        }
        DrawItemExtras(i);

        // Right-click context menu per item
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Copy"))
                nodeToCopy = i;
            if (CanDeleteItem(i)) {
                if (ImGui::MenuItem(GetDeleteLabel()))
                    nodeToDelete = id;
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    if (nodeToCopy >= 0 && nodeToCopy < GetItemCount())
        CopyItemToClipboard(nodeToCopy);
    if (nodeToDelete != -1)
        RemoveItem(nodeToDelete);

    // Right-click empty area: Paste + Add Item
    if (ImGui::BeginPopupContextWindow("##ItemListPopup", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (!m_ClipboardData.empty()) {
            if (ImGui::MenuItem("Paste"))
                PasteItemFromClipboard();
            ImGui::Separator();
        }
        if (ImGui::MenuItem("Add Item"))
            AddItem();
        ImGui::EndPopup();
    }
}

// ============================================================================
// CopyItemToClipboard / PasteItemFromClipboard
// ============================================================================
inline void ManagerBase::CopyItemToClipboard(int index)
{
    int oldSel = GetSelectedIndex();
    SelectItem(index);  // temporarily select so ClipboardCopySelected() works
    m_ClipboardData = ClipboardCopySelected();
    SelectItem(oldSel); // restore previous selection
}

inline void ManagerBase::PasteItemFromClipboard()
{
    if (!m_ClipboardData.empty())
        ClipboardPaste(m_ClipboardData);
}
