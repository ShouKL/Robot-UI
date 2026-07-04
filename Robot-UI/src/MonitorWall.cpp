#include "MonitorWall.h"

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

    auto* stream = m_LiveStreamMgr->GetDeviceByIndex(m_ActiveStreamIdx);
    if (!stream) return;

    if (strlen(stream->ip) == 0 || strcmp(stream->ip, "0.0.0.0") == 0)
    {
        return;
    }

    if (!stream->isStreaming)
    {
        WL_INFO_TAG("MONITOR_WALL", "Opening stream: {}", stream->name);
        stream->TryOpen();  // 后台线程，不阻塞 UI
    }
    m_StreamRunning = true;
}

void MonitorWall::StopStream()
{
    if (!m_LiveStreamMgr) return;
    if (m_ActiveStreamIdx < 0 || m_ActiveStreamIdx >= m_LiveStreamMgr->GetItemCount()) return;

    auto* stream = m_LiveStreamMgr->GetDeviceByIndex(m_ActiveStreamIdx);
    if (!stream) return;

    if (stream->isStreaming)
    {
        WL_INFO_TAG("MONITOR_WALL", "Closing stream: {}", stream->name);
        stream->Close();
        stream->isStreaming = false;
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

    auto* stream = m_LiveStreamMgr->GetDeviceByIndex(m_ActiveStreamIdx);
    if (!stream || !stream->isStreaming) return;
    stream->Update();
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

    auto* stream = m_LiveStreamMgr->GetDeviceByIndex(m_ActiveStreamIdx);
    const char* name = m_LiveStreamMgr->GetItemNameBuf(m_ActiveStreamIdx);

    //    
    ImGui::TextUnformatted(name);
    ImGui::Separator();

    //     —    
    float availWidth  = ImGui::GetContentRegionAvail().x;
    float availHeight = ImGui::GetContentRegionAvail().y;
    //        
    float statusHeight = ImGui::GetFrameHeightWithSpacing();
    float imgHeight = availHeight - statusHeight - ImGui::GetStyle().ItemSpacing.y;
    ImVec2 imgSize(availWidth, imgHeight);

    ImVec2 imgPos = ImGui::GetCursorScreenPos();

    if (stream && stream->isStreaming && stream->IsReady())
    {
        void* descSet = stream->GetDescriptorSet();
        if (descSet)
        {
            // Determine UV coordinates for flip
            ImVec2 uv0 = ImVec2(m_FlipH ? 1.0f : 0.0f, m_FlipV ? 1.0f : 0.0f);
            ImVec2 uv1 = ImVec2(m_FlipH ? 0.0f : 1.0f, m_FlipV ? 0.0f : 1.0f);

            if (fabsf(m_RotationAngle) < 0.01f)
            {
                // No rotation — simple Image with flipped UVs
                ImGui::Image((ImTextureID)descSet, imgSize, uv0, uv1);
            }
            else
            {
                // Rotate around center using AddImageQuad
                ImDrawList* dl = ImGui::GetWindowDrawList();
                float cx = imgPos.x + imgSize.x * 0.5f;
                float cy = imgPos.y + imgSize.y * 0.5f;
                float rad = m_RotationAngle * 3.14159265f / 180.0f;
                float c = cosf(rad);
                float s = sinf(rad);
                float hw = imgSize.x * 0.5f;
                float hh = imgSize.y * 0.5f;

                auto rotate = [&](float x, float y) -> ImVec2 {
                    return ImVec2(cx + x * c - y * s, cy + x * s + y * c);
                };

                dl->AddImageQuad(
                    (ImTextureID)descSet,
                    rotate(-hw, -hh), rotate( hw, -hh),
                    rotate( hw,  hh), rotate(-hw,  hh),
                    uv0, ImVec2(uv1.x, uv0.y),
                    uv1, ImVec2(uv0.x, uv1.y));
            }
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
        const char* placeholder = stream && stream->isStreaming ? "Connecting..." : "No Signal";
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(imgPos.x + imgSize.x * 0.5f - 35.0f, imgPos.y + imgSize.y * 0.5f - 8.0f),
            IM_COL32(150, 150, 150, 255), placeholder);
    }

    //      
    if (stream && stream->isStreaming)
    {
        if (stream->IsReady())
        {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f),
                "%dx%d @ %d FPS", stream->GetWidth(), stream->GetHeight(), stream->GetFPS());
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Buffering...");
        }
    }
    else
    {
        bool hasIP = stream && (strlen(stream->ip) > 0) &&
            (strcmp(stream->ip, "0.0.0.0") != 0);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s",
            hasIP ? "Idle" : "No IP");
    }

    ImGui::End();
}
