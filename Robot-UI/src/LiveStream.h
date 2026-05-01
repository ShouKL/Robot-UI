#pragma once
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

// GStreamer 库（C 接口）
extern "C" {
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
}

// --- 枚举定义：用于 UI 选择和逻辑判断 ---

enum CodecType { H264 = 0, H265 = 1 };
enum StreamType { Main = 0, Sub = 1 }; // 主码流（高画质）或辅码流（流畅）
enum DecoderType {
    SOFTWARE = 0,    // CPU 软解 (libav)
    NVIDIA_HW = 1,  // NVIDIA NVDEC 硬件加速
    D3D11_VA = 2,   // Windows D3D11 硬件加速
    INTEL_QSV = 3   // Intel QuickSync 硬件加速
};
enum TransportProto { TCP = 0, UDP = 1 }; // RTSP 传输协议
enum BufferMode { AUTO = 0, SLAVE = 1, BUFFER = 2, SYNC = 3 }; // GStreamer 缓冲模式
enum CameraBrand { HIKVISION = 0, DAHUA = 1, CUSTOM = 2 }; // 预设不同厂家的 RTSP 路径

// --- 视频流配置结构体 ---
struct StreamConfig
{
    // 基本连接信息
    char name[128] = "Camera_01";
    char ip[64] = "";
    char user[64] = "admin";
    char pass[64] = "123456";
    int port = 554;

    // 协议与码流设置
    CameraBrand brand = CameraBrand::HIKVISION;
    int channel = 1;
    CodecType codec = CodecType::H264;
    StreamType streamType = StreamType::Main;
    TransportProto protocol = TransportProto::UDP;
    // 自定义 RTSP 路径模板（如：/Streaming/Channels/101）
    char customPath[256] = "/{codec}/ch{channel}/{streamType}/av_stream";

    // 网络与延迟优化
    int latency = 0;              // 抖动缓冲区延迟 (ms)
    int udpBufferSize = 8388608;  // UDP 接收缓冲区大小 (8MB)
    int timeout = 5000000;        // 超时时间 (微秒)
    bool dropOnLatency = true;    // 延迟过高时是否丢帧
    bool ntpSync = false;         // 是否使用 NTP 时间戳同步
    BufferMode bufferMode = BufferMode::AUTO;

    // 解码与渲染性能设置
    DecoderType decoder = DecoderType::NVIDIA_HW;
    int cpuThreads = 4;           // 软解时的 CPU 线程数
    bool syncToClock = false;     // 是否与系统时钟同步播放
    int maxBuffers = 1;           // 最大缓冲帧数（1 表示极致低延迟）
    bool lowLatencyMode = true;   // 开启低延迟解码优化
    bool useBGRA = false;         // 输出格式（BGRA 或 RGBA）
    bool autoHardwareFallback = true; // 硬件解码失败是否自动转软解
};

// --- 视频流处理类 ---
class LiveStream {
public:
    LiveStream();
    ~LiveStream();

    // 控制接口
    bool Open(const StreamConfig& config); // 根据配置初始化并启动 GStreamer 管道
    void Close();                          // 停止并释放资源
    void Update();                         // 每帧调用：将解码数据上传到 GPU 纹理

    // UI 相关接口
    void* GetDescriptorSet() const { return m_image ? m_image->GetDescriptorSet() : nullptr; } // 获取 ImGui 可用的纹理 ID
    bool IsReady() const { return m_image != nullptr; }

    // 状态获取
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetFPS() const { return m_currentFPS; }
    std::string GetLastErrorMsg() const { return m_lastErrorMsg; }

    // ImGui 绘制面板接口
    void DrawStreamConfigPanel();
    const StreamConfig& GetStreamConfig() const { return m_StreamConfig; }
    StreamConfig& GetStreamConfig() { return m_StreamConfig; }

private:
    // 内部逻辑
    std::string BuildPipelineString(const StreamConfig& config); // 动态生成 GStreamer 启动字符串

    // 细分的 UI 绘制函数
    void DrawConnectionSettings();
    void DrawProtocolCodecSettings();
    void DrawNetworkBufferSettings();
    void DrawDecoderRenderingSettings();

private:
    StreamConfig m_StreamConfig;

    // GStreamer 核心组件
    GstElement* m_pipeline = nullptr;
    // 回调函数：当 GStreamer 解码出一帧数据时被触发
    static GstFlowReturn OnNewSample(GstAppSink* sink, gpointer user_data);

    // 线程安全与缓冲
    std::mutex m_mutex;
    std::vector<uint8_t> m_pixels;       // 存储原始像素数据 (CPU)
    std::vector<uint8_t> m_localBuffer;  // 临时交换缓冲

    int m_width = 0, m_height = 0;
    bool m_hasNewFrame = false;          // 是否有新数据待上传

    // 统计数据
    int m_frameCount = 0;
    int m_currentFPS = 0;
    double m_lastFpsTime = 0.0;
    std::string m_lastErrorMsg;

    // GPU 纹理对象（由 Walnut 库封装）
    std::shared_ptr<Walnut::Image> m_image = nullptr;
};