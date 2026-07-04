#pragma once
#include "ManagerBase.h"
#include "LiveStream.h"

class LiveStreamManager : public ManagerBase {
public:
    LiveStreamManager();
    ~LiveStreamManager();

    void AddItem() override;
    void RemoveItem(int id) override;
    void RenameItem(int id, const char* newName) override;

    LiveStream* GetSelectedDevice();
    LiveStream* GetDeviceByIndex(int index);

    int    GetItemCount() const override { return (int)m_devices.size(); }
    int    GetItemId(int index) const override { return m_devices[index]->id; }
    char*  GetItemNameBuf(int index) override { return m_devices[index]->name; }
    size_t GetItemNameBufSize(int index) const override { return sizeof(m_devices[index]->name); }
    bool   IsItemSelected(int index) const override { return m_devices[index]->isSelected; }
    int    GetSelectedIndex() const override;
    void   SelectItem(int index) override;
    const char* GetDeleteLabel() const override { return "Delete Device"; }

    //   RobotSettingPanel    使用
    void DrawContent() override;

    std::string ClipboardCopySelected() override;
    void        ClipboardPaste(const std::string& yaml) override;

    //    ：获取所有设备的流配置（以 LiveStream 副本形式）
    std::vector<LiveStream> GetAllItems() const;

    //        （替换所有现有设备）
    void LoadItems(const std::vector<LiveStream>& configs);

    void ResetToDefault();

private:
    // ======== UI    ========
    static void DrawPropertyLabel(const char* label);
    void DrawConnectionSettings(LiveStream& stream);
    void DrawProtocolCodecSettings(LiveStream& stream);
    void DrawNetworkBufferSettings(LiveStream& stream);
    void DrawDecoderRenderingSettings(LiveStream& stream);
    void DrawNoticePanel(LiveStream& stream);
    void DrawStreamConfigPanel(LiveStream& stream);

    std::vector<std::unique_ptr<LiveStream>> m_devices;
};