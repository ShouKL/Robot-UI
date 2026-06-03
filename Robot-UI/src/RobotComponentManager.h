
#pragma once

#include "ManagerBase.h"
#include "RobotComponent.h"
#include <vector>

class RobotComponentManager : public ManagerBase
{
public:
    RobotComponentManager();
    ~RobotComponentManager() = default;

    void AddItem() override;
    void RemoveItem(int id) override;
    void RenameItem(int id, const char* newName) override;

    void SetSelectedIndex(int idx);
    RobotComponent* GetSelectedComponent();

    void LoadComponents(const std::vector<RobotMode>& items, int selectedIdx);
    void LoadItems(const std::vector<RobotMode>& items);
    std::vector<RobotComponent>& GetComponents() { return m_Components; }
    const std::vector<RobotComponent>& GetComponents() const { return m_Components; }
    std::vector<RobotMode> GetAllItems() const;

    int    GetItemCount() const override { return (int)m_Components.size(); }
    int    GetItemId(int index) const override { return m_Components[index].id; }
    char*  GetItemNameBuf(int index) override { return m_Components[index].component.name; }
    int    GetSelectedIndex() const override { return m_SelectedIndex; }
    void   SelectItem(int index) override { SetSelectedIndex(index); }

    void DrawContent() override;

private:
    void DeleteByIndex(int index);

    std::vector<RobotComponent> m_Components;
    int  m_SelectedIndex = 0;
};
