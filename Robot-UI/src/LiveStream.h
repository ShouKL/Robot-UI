#pragma once
#include "EditDraftBase.h"
#include <iostream>
#include <vector>
#include <mutex>
#include <string>
#include <memory>
#include <sstream>
#include <chrono>
#include <windows.h>
#include <utility>
#include "imgui.h"
#include "Walnut/Image.h"

// GStreamer C
extern "C" {
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
}

// ---  UI  ---
enum CodecType { H264 = 0, H265 = 1 };
enum StreamType { Main = 0, Sub = 1 }; //
enum DecoderType {
    SOFTWARE = 0,    // CPU  (libav)
    NVIDIA_HW = 1,  // NVIDIA NVDEC
    D3D11_VA = 2,   // Windows D3D11
    INTEL_QSV = 3   // Intel QuickSync
};

enum TransportProto { TCP = 0, UDP = 1 }; // RTSP
enum BufferMode { AUTO = 0, SLAVE = 1, BUFFER = 2, SYNC = 3 }; // GStreamer
enum CameraBrand { HIKVISION = 0, DAHUA = 1, CUSTOM = 2 };

struct StreamConfig
{
    char name[128] = "";
    char ip[64] = "";
    char user[64] = "";
    char pass[64] = "";
    int port = 0;
    CameraBrand brand = CameraBrand::CUSTOM;
    int channel = 1;
    CodecType codec = CodecType::H264;
    StreamType streamType = StreamType::Main;
    TransportProto protocol = TransportProto::TCP;
    char customPath[256] = "";
    int latency = 0;
    int udpBufferSize = 0;
    int timeout = 5000000;
    bool dropOnLatency = true;
    bool ntpSync = false;
    BufferMode bufferMode = BufferMode::AUTO;
    DecoderType decoder = DecoderType::SOFTWARE;
    int cpuThreads = 0;
    bool syncToClock = false;
    int maxBuffers = 0;
    bool lowLatencyMode = true;
    bool useBGRA = false;
    bool autoHardwareFallback = true;
};

class LiveStream : public EditDraftBase {
public:
    LiveStream();
    ~LiveStream();
    bool Open(const StreamConfig& config); //  GStreamer
    void Close();                          //
    void Update();                         //  GPU
    // UI
    void* GetDescriptorSet() const { return m_image ? m_image->GetDescriptorSet() : nullptr; } //  ImGui  ID
    bool IsReady() const { return m_image != nullptr; }
    //
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetFPS() const { return m_currentFPS; }
    std::string GetLastErrorMsg() const { return m_lastErrorMsg; }
    // ImGui
    void DrawStreamConfigPanel();
    const StreamConfig& GetStreamConfig() const { return m_StreamConfig; }
    StreamConfig& GetStreamConfig() { return m_StreamConfig; }
private:
    //
    std::string BuildPipelineString(const StreamConfig& config); //  GStreamer
    //  UI
    void DrawConnectionSettings();
    void DrawProtocolCodecSettings();
    void DrawNetworkBufferSettings();
    void DrawDecoderRenderingSettings();
private:
    StreamConfig m_StreamConfig;
    // GStreamer
    GstElement* m_pipeline = nullptr;
    //  GStreamer
    static GstFlowReturn OnNewSample(GstAppSink* sink, gpointer user_data);
    //
    std::mutex m_mutex;
    std::vector<uint8_t> m_pixels;       //  (CPU)
    std::vector<uint8_t> m_localBuffer;  //
    int m_width = 0, m_height = 0;
    bool m_hasNewFrame = false;          //
    //
    int m_frameCount = 0;
    int m_currentFPS = 0;
    double m_lastFpsTime = 0.0;
    std::string m_lastErrorMsg;
    // GPU  Walnut
    std::shared_ptr<Walnut::Image> m_image = nullptr;
};
