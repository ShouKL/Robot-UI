#pragma once

class EditDraftBase {
public:
    virtual ~EditDraftBase() = default;

    bool IsEditing() const { return m_IsEditing; }
    virtual void BeginEdit() { m_IsEditing = true; }
    virtual void ApplyEdit() { m_IsEditing = false; }
    virtual void CancelEdit() { m_IsEditing = false; }

protected:
    bool m_IsEditing = false;
};
