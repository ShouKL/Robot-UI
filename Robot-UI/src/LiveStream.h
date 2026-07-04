#pragma once
#include "core.h"
#include "Walnut/Image.h"
#include <chrono>

// GStreamer C
extern "C" {
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
}

// ---  UI   ---
enum CodecType { H264 = 0, H265 = 1, H265_PLUS = 2 };
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

// ============================================================================
// LiveStream — 流数据 + GStreamer   （不含 UI 绘制，UI 在 LiveStreamManager）
// 合并原 StreamConfig / StreamNode / DeviceNode 的字段
// ============================================================================
class LiveStream {
public:
    // ========   ========
    int  id          = 0;
    std::atomic<bool> isStreaming{false};
    bool isSelected  = false;

    // ========   （原 StreamConfig 字段）========
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

    // ======== 构造 / 析构 / 拷贝 / 移动 ========
    LiveStream();
    ~LiveStream();

    // 显式拷贝（跳过互斥锁和运行时状态，仅拷贝配置数据用于序列化）
    LiveStream(const LiveStream& other);
    LiveStream& operator=(const LiveStream& other);

    // 显式移动：因 std::mutex 不可移动，实现为拷贝 + 默认构造 mutex
    LiveStream(LiveStream&& other);
    LiveStream& operator=(LiveStream&& other);

    // ========   ========
    bool Open();                        // 同步单次尝试，由 TryOpen 后台线程调用
    void TryOpen(int maxRetries = 2, int intervalMs = 800);  // 非阻塞，后台线程重试
    void Close();
    void CancelConnect();  // 取消正在进行的后台连接
    void Update();

    // ========  状态查询 ========
    bool IsConnecting() const { return m_connecting.load(std::memory_order_relaxed); }
    int  GetConnectAttempt() const { return m_connectAttempt.load(std::memory_order_relaxed); }
    int  GetConnectTotal()   const { return m_connectTotal.load(std::memory_order_relaxed); }

    // ========   ========
    void* GetDescriptorSet() const { return m_image ? m_image->GetDescriptorSet() : nullptr; }
    bool IsReady() const { return m_image != nullptr; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetFPS() const { return m_currentFPS; }
    std::string GetLastErrorMsg() const { return m_lastErrorMsg; }

    // ======== GStreamer    ========
    std::string BuildPipelineString() const;
    std::string BuildCLIReferenceString() const;

private:
    static GstFlowReturn OnNewSample(GstAppSink* sink, gpointer user_data);
    void CloseInternal(GstClockTime timeout);  // 带超时的内部拆卸

    // ======== GStreamer   ========
    GstElement* m_pipeline = nullptr;
    std::mutex m_pipeMutex;  // 保护 m_pipeline（后台线程 Open / UI 线程 Update & Close）
    std::atomic<bool> m_destroying{false};  // 析构标志，保护 TryOpen 后台线程
    std::atomic<bool> m_connecting{false};  // UI 显示 & 后台循环检查
    std::atomic<int>  m_connectAttempt{0};   // 当前尝试次数（1-based，供 UI 显示）
    std::atomic<int>  m_connectTotal{0};     // 总尝试次数
    std::atomic<int>  m_connectGeneration{0};// 每次 TryOpen 递增，旧线程检测到不再匹配则退出
    std::atomic<bool> m_hasError{false};     // Update 检测到错误，标记需要 Close
    std::chrono::steady_clock::time_point m_connectingStart{};  // 连接开始时间戳（用于最小显示时间）

    // ========    ========
    std::mutex m_mutex;
    std::vector<uint8_t> m_pixels;       // CPU   
    std::vector<uint8_t> m_localBuffer;
    int m_width = 0, m_height = 0;
    bool m_hasNewFrame = false;

    // ======== FPS   ========
    int m_frameCount = 0;
    int m_currentFPS = 0;
    double m_lastFpsTime = 0.0;
    std::string m_lastErrorMsg;

    // ======== GPU  (Walnut) ========
    std::shared_ptr<Walnut::Image> m_image = nullptr;
};
