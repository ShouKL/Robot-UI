
#pragma once

#include "ManagerBase.h"
#include "GamepadMapper.h"
#include <cstring>
#include <string>
#include <vector>

class GamepadMapperManager : public ManagerBase
{
public:
    GamepadMapperManager();
    ~GamepadMapperManager() = default;

    void AddItem() override;
    void RemoveItem(int id) override;

    int  GetSelectedIndex() const { return m_SelectedIndex; }
    void SetSelectedIndex(int idx);
    GamepadMapper* GetSelectedMapper();

    void LoadMappers(const std::vector<GamepadMapper>& items, int selectedIdx);
    void RestoreMappers(const std::vector<GamepadMapper>& items);
    std::vector<GamepadMapper>& GetMappers() { return m_Mappers; }
    std::vector<GamepadMapper> GetAllMappers() const;

    void DrawItemList(float width) override;
    void DrawContent() override;
    void DrawContent(float availableHeight);

private:
    void DeleteByIndex(int index);

    std::vector<GamepadMapper> m_Mappers;
    int m_SelectedIndex = 0;
};
