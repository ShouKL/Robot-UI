#include "MonitorWall.h"
#include "LiveStreamManager.h"
#include "LiveStream.h"
#include "Walnut/Core/Log.h"

MonitorWall::MonitorWall()
{
    WL_INFO_TAG("MONITOR_WALL", "MonitorWall created");
}

MonitorWall::~MonitorWall()
{
    StopStream();
}

void MonitorWall::StartStream()
{
    if (!m_LiveStreamMgr) return;
    if (m_StreamRunning) return;
    if (m_ActiveStreamIdx < 0 || m_ActiveStreamIdx >= m_LiveStreamMgr->GetItemCount()) return;

    auto* dev = m_LiveStreamMgr->GetDeviceByIndex(m_ActiveStreamIdx);
    if (!dev) return;

    const auto& cfg = dev->stream->GetStreamConfig();
    if (strlen(cfg.ip) == 0 || strcmp(cfg.ip, "0.0.0.0") == 0)
    {
        return;
    }

    if (!dev->isStreaming)
    {
        WL_INFO_TAG("MONITOR_WALL", "Opening stream: {}", cfg.name);
        if (dev->stream->Open(cfg))
        {
            dev->isStreaming = true;
        }
        else
        {
            WL_WARN_TAG("MONITOR_WALL", "Failed to open stream: {} ({})", cfg.name, dev->stream->GetLastErrorMsg());
        }
    }
    m_StreamRunning = true;
}

void MonitorWall::StopStream()
{
    if (!m_LiveStreamMgr) return;
    if (m_ActiveStreamIdx < 0 || m_ActiveStreamIdx >= m_LiveStreamMgr->GetItemCount()) return;

    auto* dev = m_LiveStreamMgr->GetDeviceByIndex(m_ActiveStreamIdx);
    if (!dev) return;

    if (dev->isStreaming)
    {
        WL_INFO_TAG("MONITOR_WALL", "Closing stream: {}", dev->stream->GetStreamConfig().name);
        dev->stream->Close();
        dev->isStreaming = false;
    }
    m_StreamRunning = false;
}

void MonitorWall::ConnectStream()
{
    StartStream();
}

void MonitorWall::DisconnectStream()
{
    StopStream();
}

void MonitorWall::UpdateStream()
{
    if (!m_LiveStreamMgr) return;
    if (m_ActiveStreamIdx < 0 || m_ActiveStreamIdx >= m_LiveStreamMgr->GetItemCount()) return;

    auto* dev = m_LiveStreamMgr->GetDeviceByIndex(m_ActiveStreamIdx);
    if (!dev || !dev->isStreaming) return;
    dev->stream->Update();
}

void MonitorWall::Draw(bool* p_open)
{
    if (!ImGui::Begin("Monitor Wall", p_open))
    {
        ImGui::End();
        return;
    }

    // 仅每帧拉取新帧，不自动启动流
    UpdateStream();

    if (!m_LiveStreamMgr || m_LiveStreamMgr->GetItemCount() == 0)
    {
        ImGui::TextDisabled("No live stream devices configured.");
        ImGui::End();
        return;
    }

    if (m_ActiveStreamIdx < 0 || m_ActiveStreamIdx >= m_LiveStreamMgr->GetItemCount())
    {
        ImGui::TextDisabled("No live stream selected in Robot Status.");
        ImGui::End();
        return;
    }

    auto* dev = m_LiveStreamMgr->GetDeviceByIndex(m_ActiveStreamIdx);
    const char* name = m_LiveStreamMgr->GetItemNameBuf(m_ActiveStreamIdx);

    // 设备名称
    ImGui::TextUnformatted(name);
    ImGui::Separator();

    // 视频画面 — 填满窗口
    float availWidth  = ImGui::GetContentRegionAvail().x;
    float availHeight = ImGui::GetContentRegionAvail().y;
    // 为底部状态栏预留高度
    float statusHeight = ImGui::GetFrameHeightWithSpacing();
    float imgHeight = availHeight - statusHeight - ImGui::GetStyle().ItemSpacing.y;
    ImVec2 imgSize(availWidth, imgHeight);

    ImVec2 imgPos = ImGui::GetCursorScreenPos();

    if (dev && dev->isStreaming && dev->stream->IsReady())
    {
        void* descSet = dev->stream->GetDescriptorSet();
        if (descSet)
        {
            ImGui::Image((ImTextureID)descSet, imgSize);
        }
        else
        {
            ImGui::Dummy(imgSize);
            ImGui::GetWindowDrawList()->AddRectFilled(
                imgPos, ImVec2(imgPos.x + imgSize.x, imgPos.y + imgSize.y),
                IM_COL32(20, 20, 20, 255));
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(imgPos.x + imgSize.x * 0.5f - 40.0f, imgPos.y + imgSize.y * 0.5f - 8.0f),
                IM_COL32(150, 150, 150, 255), "Waiting...");
        }
    }
    else
    {
        ImGui::Dummy(imgSize);
        ImGui::GetWindowDrawList()->AddRectFilled(
            imgPos, ImVec2(imgPos.x + imgSize.x, imgPos.y + imgSize.y),
            IM_COL32(30, 30, 30, 255));
        const char* placeholder = dev && dev->isStreaming ? "Connecting..." : "No Signal";
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(imgPos.x + imgSize.x * 0.5f - 35.0f, imgPos.y + imgSize.y * 0.5f - 8.0f),
            IM_COL32(150, 150, 150, 255), placeholder);
    }

    // 底部状态栏
    if (dev && dev->isStreaming)
    {
        if (dev->stream->IsReady())
        {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f),
                "%dx%d @ %d FPS", dev->stream->GetWidth(), dev->stream->GetHeight(), dev->stream->GetFPS());
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Buffering...");
        }
    }
    else
    {
        bool hasIP = dev && (strlen(dev->stream->GetStreamConfig().ip) > 0) &&
            (strcmp(dev->stream->GetStreamConfig().ip, "0.0.0.0") != 0);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s",
            hasIP ? "Idle" : "No IP");
    }

    ImGui::End();
}
