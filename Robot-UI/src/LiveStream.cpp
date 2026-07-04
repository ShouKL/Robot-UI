#include "LiveStream.h"
#include <thread>
#include <chrono>

// ======== GStreamer 安全初始化（启动时检测，不延迟）========
static bool s_gst_available = false;

static void EnsureGstInit()
{
    static bool s_inited = false;
    if (s_inited) return;
    s_inited = true;

    WL_INFO_TAG("GSTREAMER", "Initializing GStreamer...");

    // 防止 gst_init 扫描插件时卡死（Windows 上最常见的卡死根因）
    _putenv_s("GST_REGISTRY_FORK_DISABLE", "1");
    _putenv_s("GST_PLUGIN_SCANNER", "gst-plugin-scanner");

    // 便携式分发：检测 exe 同目录下的 gstreamer-1.0/ 插件目录
    // 若存在则设置 GST_PLUGIN_PATH + GST_REGISTRY，解决：
    //  - "no element rtspsrc"（插件找不到）
    //  - 系统 GStreamer 注册表缓存冲突（registry 路径冲突）
    {
        char exePath[MAX_PATH];
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::string exeDir(exePath);
        size_t pos = exeDir.find_last_of("\\/");
        if (pos != std::string::npos)
            exeDir = exeDir.substr(0, pos);

        std::string localPluginPath = exeDir + "\\gstreamer-1.0";
        DWORD attr = GetFileAttributesA(localPluginPath.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
        {
            // 使用本地插件目录
            _putenv_s("GST_PLUGIN_PATH", localPluginPath.c_str());
            WL_INFO_TAG("GSTREAMER", "Using local plugins: {}", localPluginPath);

            // 使用本地 registry 文件，避免与系统 GStreamer 注册表冲突
            std::string localRegPath = exeDir + "\\gstreamer-1.0\\registry.x86_64.bin";
            _putenv_s("GST_REGISTRY", localRegPath.c_str());
            WL_INFO_TAG("GSTREAMER", "Using local registry: {}", localRegPath);
        }
    }

    WL_INFO_TAG("GSTREAMER", "Calling gst_init_check...");
    GError* gstErr = nullptr;
    if (!gst_init_check(nullptr, nullptr, &gstErr))
    {
        WL_ERROR_TAG("GSTREAMER", "gst_init failed: {}", gstErr ? gstErr->message : "unknown");
        if (gstErr) g_error_free(gstErr);
        return;
    }
    WL_INFO_TAG("GSTREAMER", "GStreamer initialized successfully");
    s_gst_available = true;
}
LiveStream::LiveStream()
{
    // 启动时检测 GStreamer 环境 — 有 env var 防护不会卡死，失败立即打日志
    EnsureGstInit();
}

LiveStream::~LiveStream()
{
    m_destroying = true;  // 阻止 TryOpen 后台线程继续访问 this
    // 析构时用较长超时确保 pipeline 释放，但不无限等待
    CloseInternal(5 * GST_SECOND);
}

// 显式拷贝：仅拷贝配置字段（跳过 mutex / pipeline / 运行时状态）
LiveStream::LiveStream(const LiveStream& other)
{
    id = other.id;
    isStreaming = false;  // 拷贝后不保留流状态
    isSelected  = other.isSelected;
    strncpy_s(name, other.name, sizeof(name) - 1);
    strncpy_s(ip, other.ip, sizeof(ip) - 1);
    strncpy_s(user, other.user, sizeof(user) - 1);
    strncpy_s(pass, other.pass, sizeof(pass) - 1);
    port       = other.port;
    brand      = other.brand;
    channel    = other.channel;
    codec      = other.codec;
    streamType = other.streamType;
    protocol   = other.protocol;
    strncpy_s(customPath, other.customPath, sizeof(customPath) - 1);
    latency       = other.latency;
    udpBufferSize = other.udpBufferSize;
    timeout       = other.timeout;
    dropOnLatency = other.dropOnLatency;
    ntpSync       = other.ntpSync;
    bufferMode    = other.bufferMode;
    decoder       = other.decoder;
    cpuThreads    = other.cpuThreads;
    syncToClock   = other.syncToClock;
    maxBuffers    = other.maxBuffers;
    lowLatencyMode = other.lowLatencyMode;
    useBGRA       = other.useBGRA;
    autoHardwareFallback = other.autoHardwareFallback;
    // m_mutex: default-constructed (mutex is non-copyable)
    // m_pipeline, m_pixels, m_image: default (no runtime state)
}

LiveStream& LiveStream::operator=(const LiveStream& other)
{
    if (this == &other) return *this;
    Close();
    id = other.id;
    isStreaming = false;
    isSelected  = other.isSelected;
    strncpy_s(name, other.name, sizeof(name) - 1);
    strncpy_s(ip, other.ip, sizeof(ip) - 1);
    strncpy_s(user, other.user, sizeof(user) - 1);
    strncpy_s(pass, other.pass, sizeof(pass) - 1);
    port       = other.port;
    brand      = other.brand;
    channel    = other.channel;
    codec      = other.codec;
    streamType = other.streamType;
    protocol   = other.protocol;
    strncpy_s(customPath, other.customPath, sizeof(customPath) - 1);
    latency       = other.latency;
    udpBufferSize = other.udpBufferSize;
    timeout       = other.timeout;
    dropOnLatency = other.dropOnLatency;
    ntpSync       = other.ntpSync;
    bufferMode    = other.bufferMode;
    decoder       = other.decoder;
    cpuThreads    = other.cpuThreads;
    syncToClock   = other.syncToClock;
    maxBuffers    = other.maxBuffers;
    lowLatencyMode = other.lowLatencyMode;
    useBGRA       = other.useBGRA;
    autoHardwareFallback = other.autoHardwareFallback;
    return *this;
}

// 显式移动：拷贝配置字段，运行时成员（mutex/pipeline/pixels）默认构造
LiveStream::LiveStream(LiveStream&& other)
    : LiveStream()
{
    *this = static_cast<const LiveStream&>(other);
}

LiveStream& LiveStream::operator=(LiveStream&& other)
{
    if (this == &other) return *this;
    return operator=(static_cast<const LiveStream&>(other));
}

// 返回用于 RTSP URL 路径的 codec 字符串
static std::string GetCodecUrlStr(const LiveStream& stream) {
    switch (stream.codec) {
        case CodecType::H265:       return "h265";
        case CodecType::H265_PLUS:  return "h265+";
        default:                    return "h264";
    }
}

// 返回用于 GStreamer 管线元素的 codec 字符串（H.265+ 解码器与 H.265 相同）
static std::string GetCodecPipelineStr(const LiveStream& stream) {
    switch (stream.codec) {
        case CodecType::H265:
        case CodecType::H265_PLUS:  return "h265";
        default:                    return "h264";
    }
}

// ======== GStreamer 管线构建 ========

// 构建 rtspsrc 的 RTSP URL 路径
static std::string BuildRTSPSrcPath(const LiveStream& stream, const std::string& codecStr) {
    if (stream.brand == CameraBrand::HIKVISION) {
        return "/" + codecStr + "/ch" + std::to_string(stream.channel) + "/" +
            (stream.streamType == StreamType::Main ? "main" : "sub") + "/av_stream";
    }
    else if (stream.brand == CameraBrand::DAHUA) {
        return "/cam/realmonitor?channel=" + std::to_string(stream.channel) +
            "&subtype=" + (stream.streamType == StreamType::Main ? "0" : "1");
    }
    else {
        std::string path = stream.customPath;
        size_t pos;
        while ((pos = path.find("{codec}")) != std::string::npos) path.replace(pos, 7, codecStr);
        while ((pos = path.find("{channel}")) != std::string::npos) path.replace(pos, 9, std::to_string(stream.channel));
        while ((pos = path.find("{streamType}")) != std::string::npos) path.replace(pos, 12, (stream.streamType == StreamType::Main ? "main" : "sub"));
        return path;
    }
}

// 构建 rtspsrc 的属性字符串（仅包含非默认值的参数）
static std::string BuildRTSPSrcProperties(const LiveStream& stream) {
    std::stringstream ss;

    // 基础属性 — 始终输出
    ss << "protocols=" << (stream.protocol == TransportProto::TCP ? "tcp" : "udp");
    ss << " latency=" << stream.latency;

    // 可选属性 — 仅在非默认值时输出，让 GStreamer 使用自身默认值
    if (stream.bufferMode != BufferMode::AUTO)
        ss << " buffer-mode=" << (int)stream.bufferMode;
    if (stream.dropOnLatency)
        ss << " drop-on-latency=true";
    if (stream.timeout != 5000000)
        ss << " timeout=" << stream.timeout;
    if (stream.udpBufferSize > 0)
        ss << " udp-buffer-size=" << stream.udpBufferSize;
    if (stream.ntpSync)
        ss << " ntp-sync=true";

    return ss.str();
}

// 构建解码+转换管线（appsink 路径）
static std::string BuildDecoderPipeline(const LiveStream& stream, const std::string& codecStr) {
    std::stringstream ss;
    std::string renderFormat = stream.useBGRA ? "BGRA" : "RGBA";

    ss << "rtp" << codecStr << "depay ! " << codecStr << "parse ! ";

    if (stream.decoder == DecoderType::D3D11_VA) {
        ss << "d3d11" << codecStr << "dec ! d3d11convert ! video/x-raw(memory:D3D11Memory),format=" << renderFormat << " ! d3d11download ! video/x-raw,format=" << renderFormat << " ! ";
    }
    else if (stream.decoder == DecoderType::NVIDIA_HW) {
        ss << "nv" << codecStr << "dec ! videoconvert ! video/x-raw,format=" << renderFormat << " ! ";
    }
    else if (stream.decoder == DecoderType::INTEL_QSV) {
        ss << "qsv" << codecStr << "dec ! videoconvert ! video/x-raw,format=" << renderFormat << " ! ";
    }
    else {
        ss << "avdec_" << codecStr << " max-threads=" << stream.cpuThreads << " ! videoconvert ! video/x-raw,format=" << renderFormat << " ! ";
    }

    // appsink — 将帧传入 CPU 内存供 ImGui 渲染
    ss << "appsink name=mysink emit-signals=true "
        << "sync=" << (stream.syncToClock ? "true" : "false") << " "
        << "max-buffers=" << stream.maxBuffers << " "
        << "drop=true";
    return ss.str();
}

// 构建直接 GPU 渲染管线（d3d11videosink，用于 CLI 参考）
static std::string BuildDirectRenderPipeline(const LiveStream& stream, const std::string& codecStr) {
    std::stringstream ss;
    ss << "rtp" << codecStr << "depay ! " << codecStr << "parse ! ";

    if (stream.decoder == DecoderType::D3D11_VA) {
        ss << "d3d11" << codecStr << "dec ! d3d11videosink sync=false";
    }
    else if (stream.decoder == DecoderType::NVIDIA_HW) {
        ss << "nv" << codecStr << "dec ! d3d11videosink sync=false";
    }
    else if (stream.decoder == DecoderType::INTEL_QSV) {
        ss << "qsv" << codecStr << "dec ! d3d11videosink sync=false";
    }
    else {
        ss << "avdec_" << codecStr << " ! d3d11videosink sync=false";
    }
    return ss.str();
}

std::string LiveStream::BuildPipelineString() const {
    std::string urlCodecStr    = GetCodecUrlStr(*this);
    std::string pipelineStr    = GetCodecPipelineStr(*this);
    std::string path = BuildRTSPSrcPath(*this, urlCodecStr);

    // 认证信息 (user:pass@)
    std::string creds;
    if (strlen(user) > 0) {
        creds = user;
        if (strlen(pass) > 0) creds += ":" + std::string(pass);
        creds += "@";
    }

    std::stringstream ss;
    ss << "rtspsrc location=\"rtsp://" << creds << ip << ":" << port << path << "\" ";
    ss << BuildRTSPSrcProperties(*this);
    ss << " ! ";
    ss << BuildDecoderPipeline(*this, pipelineStr);
    return ss.str();
}

// 生成 CLI 参考命令（使用 d3d11videosink 直接渲染，最低延迟）
std::string LiveStream::BuildCLIReferenceString() const {
    std::string urlCodecStr    = GetCodecUrlStr(*this);
    std::string pipelineStr    = GetCodecPipelineStr(*this);
    std::string path = BuildRTSPSrcPath(*this, urlCodecStr);

    // 认证信息
    std::string creds;
    if (strlen(user) > 0) {
        creds = user;
        if (strlen(pass) > 0) creds += ":" + std::string(pass);
        creds += "@";
    }

    std::stringstream ss;
    ss << "gst-launch-1.0.exe -v rtspsrc location=\"rtsp://" << creds << ip << ":" << port << path << "\" ";
    ss << BuildRTSPSrcProperties(*this);
    ss << " ! ";
    ss << BuildDirectRenderPipeline(*this, pipelineStr);
    return ss.str();
}

bool LiveStream::Open() {
    if (!s_gst_available)
    {
        m_lastErrorMsg = "GStreamer not available (init failed at startup)";
        return false;
    }

    m_lastErrorMsg.clear();
    std::string fullPipeline = BuildPipelineString();

    GError* error = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_pipeMutex);
        m_pipeline = gst_parse_launch(fullPipeline.c_str(), &error);
    }
    if (error || !m_pipeline) {
        if (error) {
            m_lastErrorMsg = error->message;
            g_error_free(error);
        }
        return false;
    }

    // 绑定 appsink 回调
    {
        std::lock_guard<std::mutex> lock(m_pipeMutex);
        GstElement* sink = gst_bin_get_by_name(GST_BIN(m_pipeline), "mysink");
        if (sink) {
            g_signal_connect(sink, "new-sample", G_CALLBACK(OnNewSample), this);
            gst_object_unref(sink);
        }
    }

    GstStateChangeReturn stateRet;
    {
        std::lock_guard<std::mutex> lock(m_pipeMutex);
        stateRet = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    }

    if (stateRet == GST_STATE_CHANGE_FAILURE) {
        std::lock_guard<std::mutex> lock(m_pipeMutex);
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        m_lastErrorMsg = "Pipeline state change failed (PLAYING)";
        return false;
    }

    // 等待最多 2.5s（rtspsrc 超时由 timeout 属性控制）
    {
        GstBus* bus;
        {
            std::lock_guard<std::mutex> lock(m_pipeMutex);
            bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
        }
        GstMessage* msg = gst_bus_timed_pop_filtered(
            bus, 2500 * GST_MSECOND,
            (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_EOS));

        if (msg) {
            if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
                GError* err = nullptr;
                gchar* debug = nullptr;
                gst_message_parse_error(msg, &err, &debug);
                m_lastErrorMsg = err ? err->message : "Unknown error";
                if (err) g_error_free(err);
                if (debug) g_free(debug);
                gst_message_unref(msg);
                gst_object_unref(bus);
                Close();
                return false;
            }
            if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
                m_lastErrorMsg = "Unexpected end of stream during connect";
                gst_message_unref(msg);
                gst_object_unref(bus);
                Close();
                return false;
            }
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(m_pipeline)) {
                GstState oldState, newState, pendingState;
                gst_message_parse_state_changed(msg, &oldState, &newState, &pendingState);
                if (newState == GST_STATE_PLAYING) {
                    gst_message_unref(msg);
                    gst_object_unref(bus);
                    return true;
                }
            }
            gst_message_unref(msg);
        }
        gst_object_unref(bus);
    }

    // 超时无结果
    WL_ERROR_TAG("LIVESTREAM", "Connect timeout (no response within 2.5s)");
    Close();
    m_lastErrorMsg = "Connection timeout — no response from camera";
    return false;
}

void LiveStream::TryOpen(int maxRetries, int intervalMs) {
    if (m_destroying.load(std::memory_order_relaxed)) return;

    // 递增 generation：旧后台线程检测到不匹配后自行退出
    int myGen = ++m_connectGeneration;
    m_connecting = true;
    m_connectTotal = maxRetries + 1;
    m_connectAttempt = 0;  // 后台线程第一轮自增到 1，UI 逐帧读取
    m_connectingStart = std::chrono::steady_clock::now();

    std::string capturedIp(ip);
    int capturedPort = port;
    int totalAttempts = maxRetries + 1;

    WL_INFO_TAG("LIVESTREAM", "Connecting to rtsp://{}:{} (max {} attempts)...",
                 capturedIp, capturedPort, totalAttempts);

    std::thread([this, totalAttempts, intervalMs, myGen, capturedIp, capturedPort]() {
        for (int attempt = 1; attempt <= totalAttempts; ++attempt) {
            // 被更新的 TryOpen 取代 → 静默退出
            if (m_connectGeneration.load(std::memory_order_relaxed) != myGen ||
                m_destroying.load(std::memory_order_relaxed)) {
                return;
            }

            m_connectAttempt = attempt;  // UI 逐帧读取显示 "Connecting... (2/4)"
            WL_INFO_TAG("LIVESTREAM", "  Attempt {} of {}...", attempt, totalAttempts);

            bool ok = Open();
            if (m_connectGeneration.load(std::memory_order_relaxed) != myGen ||
                m_destroying.load(std::memory_order_relaxed)) {
                return;
            }

            if (ok) {
                WL_INFO_TAG("LIVESTREAM", "  Attempt {} of {} OK (pipeline started)", attempt, totalAttempts);
                isStreaming = true;
                auto elapsed = std::chrono::steady_clock::now() - m_connectingStart;
                if (elapsed < std::chrono::milliseconds(300))
                    std::this_thread::sleep_for(std::chrono::milliseconds(300) - elapsed);
                m_connecting = false;
                m_connectAttempt = 0;
                return;
            }

            // Open() 在失败路径内部调用了 Close()→CancelConnect()，
            // 此时 m_connecting 和 m_connectAttempt 已被置零，需要恢复
            if (attempt < totalAttempts) {
                m_connecting = true;
                m_connectAttempt = attempt;
            }

            WL_ERROR_TAG("LIVESTREAM", "  Attempt {} of {} FAILED: {}", attempt, totalAttempts,
                         m_lastErrorMsg.empty() ? "no response" : m_lastErrorMsg);

            if (attempt < totalAttempts && !m_destroying.load(std::memory_order_relaxed))
                std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        }

        if (!m_destroying.load(std::memory_order_relaxed))
            WL_ERROR_TAG("LIVESTREAM", "All {} attempts failed.", totalAttempts);
        auto elapsed = std::chrono::steady_clock::now() - m_connectingStart;
        if (elapsed < std::chrono::milliseconds(300))
            std::this_thread::sleep_for(std::chrono::milliseconds(300) - elapsed);
        m_connecting = false;
        m_connectAttempt = 0;
    }).detach();
}

GstFlowReturn LiveStream::OnNewSample(GstAppSink* sink, gpointer user_data) {
    auto self = static_cast<LiveStream*>(user_data);
    GstSample* sample = gst_app_sink_pull_sample(sink);
    if (!sample) return GST_FLOW_OK;
    //
    GstCaps* caps = gst_sample_get_caps(sample);
    if (!caps) { gst_sample_unref(sample); return GST_FLOW_OK; }
    GstStructure* s = gst_caps_get_structure(caps, 0);
    int width = 0, height = 0;
    if (s) {
        gst_structure_get_int(s, "width", &width);
        gst_structure_get_int(s, "height", &height);
    }
    //
    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (!buffer) { gst_sample_unref(sample); return GST_FLOW_OK; }
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        //
        std::vector<uint8_t> tmp(map.size);
        memcpy(tmp.data(), map.data, map.size);
        gst_buffer_unmap(buffer, &map);
        //
        std::lock_guard<std::mutex> lock(self->m_mutex);
        self->m_width = width;
        self->m_height = height;
        self->m_pixels = std::move(tmp); //
        self->m_hasNewFrame = true;
    }
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

void LiveStream::Close() {
    CancelConnect();
    CloseInternal(100 * GST_MSECOND);
}

void LiveStream::CancelConnect() {
    m_connecting = false;  // 通知 ProcessPendingConnect 停止
    m_connectAttempt = 0;
}

void LiveStream::CloseInternal(GstClockTime timeout) {
    isStreaming = false;
    std::lock_guard<std::mutex> lock(m_pipeMutex);
    if (m_pipeline) {
        // 先发信号让 pipeline 停止接收新 buffer
        GstElement* sink = gst_bin_get_by_name(GST_BIN(m_pipeline), "mysink");
        if (sink) {
            g_signal_handlers_disconnect_by_data(sink, this);
            gst_object_unref(sink);
        }
        // 设置 NULL 状态，有超时保护
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        GstState state, pending;
        gst_element_get_state(m_pipeline, &state, &pending, timeout);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }
    m_image.reset();
}

void LiveStream::Update() {
    // 非阻塞轮询总线，捕获运行时错误
    GstBus* bus = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_pipeMutex);
        if (!m_pipeline) return;
        bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
    }
    GstMessage* msg = gst_bus_pop_filtered(bus,
        (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
    if (msg) {
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            GError* err = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_error(msg, &err, &debug);
            WL_ERROR_TAG("LIVESTREAM", "Runtime error: {} (debug: {})",
                         err ? err->message : "unknown", debug ? debug : "none");
            m_lastErrorMsg = err ? err->message : "Unknown error";
            if (err) g_error_free(err);
            if (debug) g_free(debug);
        }
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
            WL_INFO_TAG("LIVESTREAM", "End of stream");
        }
        // 检测到运行时错误 — 仅标记，不阻塞 UI（Update 在 UI 线程）
        gst_message_unref(msg);
        isStreaming = false;
        m_hasError = true;  // 标记错误，让下一帧 Close 在安全时机执行
    }
    gst_object_unref(bus);

    // 如果有待处理的错误关闭请求，在锁外执行（Close 现在 100ms 快速拆卸）
    if (m_hasError.exchange(false))
        Close();

    int w, h;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_hasNewFrame || m_pixels.empty()) return;
        m_localBuffer.swap(m_pixels); //
        w = m_width;
        h = m_height;
        m_hasNewFrame = false;
    }

    if (w <= 0 || h <= 0) return;
    //
    if (!m_image || m_image->GetWidth() != w || m_image->GetHeight() != h) {
        m_image = std::make_shared<Walnut::Image>(w, h, Walnut::ImageFormat::RGBA);
    }
    //  GPU  (Walnut )
    auto uploadStart = std::chrono::high_resolution_clock::now();
    m_image->SetData(m_localBuffer.data());
    auto uploadEnd = std::chrono::high_resolution_clock::now();
    //
    double uploadMs = std::chrono::duration<double, std::milli>(uploadEnd - uploadStart).count();
    if (uploadMs > 5.0) {
        // OutputDebugStringA  Windows
    }
    //  FPS
    m_frameCount++;
    auto now = std::chrono::high_resolution_clock::now();
    double time = std::chrono::duration<double>(now.time_since_epoch()).count();
    if (time - m_lastFpsTime >= 1.0) {
        m_currentFPS = m_frameCount;
        m_frameCount = 0;
        m_lastFpsTime = time;
    }
}
