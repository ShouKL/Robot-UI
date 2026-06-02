
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

    int  GetSelectedIndex() const { return m_SelectedIndex; }
    void SetSelectedIndex(int idx);
    RobotComponent* GetSelectedComponent();

    void LoadComponents(const std::vector<RobotMode>& items, int selectedIdx);
    void RestoreComponents(const std::vector<RobotMode>& items);
    std::vector<RobotComponent>& GetComponents() { return m_Components; }
    const std::vector<RobotComponent>& GetComponents() const { return m_Components; }
    std::vector<RobotMode> GetAllComponents() const;

    void DrawItemList(float width) override;
    void DrawContent() override;
    void DrawContent(float availableHeight);

private:
    void DeleteByIndex(int index);

    std::vector<RobotComponent> m_Components;
    int  m_SelectedIndex = 0;
};
