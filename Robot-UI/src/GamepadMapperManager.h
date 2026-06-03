
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
    void RenameItem(int id, const char* newName) override;

    void SetSelectedIndex(int idx);
    GamepadMapper* GetSelectedMapper();

    void LoadMappers(const std::vector<GamepadMapper>& items, int selectedIdx);
    void LoadItems(const std::vector<GamepadMapper>& items);
    std::vector<GamepadMapper>& GetMappers() { return m_Mappers; }
    std::vector<GamepadMapper> GetAllItems() const;

    int    GetItemCount() const override { return (int)m_Mappers.size(); }
    int    GetItemId(int index) const override { return m_Mappers[index].id; }
    char*  GetItemNameBuf(int index) override { return m_Mappers[index].name; }
    int    GetSelectedIndex() const override { return m_SelectedIndex; }
    void   SelectItem(int index) override { SetSelectedIndex(index); }

    void DrawContent() override;

private:
    void DeleteByIndex(int index);

    std::vector<GamepadMapper> m_Mappers;
    int m_SelectedIndex = 0;
};
