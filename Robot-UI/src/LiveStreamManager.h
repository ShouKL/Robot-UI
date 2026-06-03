#pragma once
#include "ManagerBase.h"
#include "LiveStream.h"

struct DeviceNode {
    int id;
    bool isStreaming = false;
    bool isSelected = false;
    std::unique_ptr<LiveStream> stream;
};

class LiveStreamManager : public ManagerBase {
public:
    LiveStreamManager();
    ~LiveStreamManager();

    void AddItem() override;
    void RemoveItem(int id) override;
    void RenameItem(int id, const char* newName) override;

    DeviceNode* GetSelectedDevice();

    int    GetItemCount() const override { return (int)m_devices.size(); }
    int    GetItemId(int index) const override { return m_devices[index].id; }
    char*  GetItemNameBuf(int index) override { return m_devices[index].stream->GetStreamConfig().name; }
    size_t GetItemNameBufSize(int index) const override { return sizeof(m_devices[index].stream->GetStreamConfig().name); }
    bool   IsItemSelected(int index) const override { return m_devices[index].isSelected; }
    void   SelectItem(int index) override;
    const char* GetDeleteLabel() const override { return "Delete Device"; }

    // 供 RobotSettingPanel 三列布局使用
    void DrawContent() override;

    // 序列化用：获取所有设备的流配置
    std::vector<StreamConfig> GetAllItems() const;

    // 从配置列表批量加载（替换所有现有设备）
    void LoadItems(const std::vector<StreamConfig>& configs);

private:
    std::vector<DeviceNode> m_devices;
    void DeleteByIndex(int index);
};