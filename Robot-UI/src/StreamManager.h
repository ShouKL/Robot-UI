#pragma once
#include "LiveStream.h"

struct DeviceNode {
    int id;
    bool isStreaming = false;
    bool isSelected = false;
    // 移除冗余的 StreamConfig config，直接复用内部 LiveStream 实例的配置
    std::unique_ptr<LiveStream> stream;
};

class StreamManager {
public:
    StreamManager();
    ~StreamManager();

    void AddDevice(const char* name, const char* url);
    void RemoveDevice(int id);

    // 每一帧在渲染循环中调用
    void UpdateAll();
    void DrawUI(const char* windowName = "Stream Manager", bool* p_open = nullptr);

private:
    std::vector<DeviceNode> m_devices;
    int m_nextId = 1000;

    // 内部 UI 组件
    void DrawDeviceTree();
    void DrawControlPanel();
    void DrawMonitorWall();
};