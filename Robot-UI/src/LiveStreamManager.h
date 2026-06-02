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

    // 每一帧在渲染循环中调用
    void UpdateAll();

    // 内联绘制（供 RobotSettingPanel 嵌入使用），不创建窗口
    void DrawContent(float availableHeight);

    // 供 RobotSettingPanel 三列布局使用
    void DrawItemList(float width) override;
    void DrawContent() override;

    // 序列化用：获取所有设备的流配置
    std::vector<StreamConfig> GetAllStreamConfigs() const;

    // 从配置列表批量加载（替换所有现有设备）
    void LoadAllConfigs(const std::vector<StreamConfig>& configs);

private:
    std::vector<DeviceNode> m_devices;
    void DeleteByIndex(int index);
};