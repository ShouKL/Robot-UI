#include "LiveStream.h"

void DrawPropertyLabel(const char* label) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label); // 左侧显示标签
    ImGui::TableNextColumn();      // 准备在右侧显示输入控件
    ImGui::SetNextItemWidth(-FLT_MIN);
}

LiveStream::LiveStream() {
    static bool inited = false;
    if (!inited) {
        gst_init(NULL, NULL);
        inited = true;
    }
}

LiveStream::~LiveStream() { Close(); }

/**
 * 构建 GStreamer 管道字符串
 * 根据配置项（品牌、编码、传输协议、解码器类型）拼接成 gst-launch 格式的字符串
 */
std::string LiveStream::BuildPipelineString(const StreamConfig& config) {
    std::stringstream ss;
    std::string codecStr = (config.codec == CodecType::H265) ? "h265" : "h264";
    std::string path;

    // 根据品牌自动生成 RTSP 路径
    if (config.brand == CameraBrand::HIKVISION) {
        // 海康格式: /h264/ch1/main/av_stream
        path = "/" + codecStr + "/ch" + std::to_string(config.channel) + "/" +
            (config.streamType == StreamType::Main ? "main" : "sub") + "/av_stream";
    }
    else if (config.brand == CameraBrand::DAHUA) {
        // 大华格式: /cam/realmonitor?channel=1&subtype=0
        path = "/cam/realmonitor?channel=" + std::to_string(config.channel) +
            "&subtype=" + (config.streamType == StreamType::Main ? "0" : "1");
    }
    else {
        // 自定义路径，支持占位符替换
        path = config.customPath;
        size_t pos;
        while ((pos = path.find("{codec}")) != std::string::npos) path.replace(pos, 7, codecStr);
        while ((pos = path.find("{channel}")) != std::string::npos) path.replace(pos, 9, std::to_string(config.channel));
        while ((pos = path.find("{streamType}")) != std::string::npos) path.replace(pos, 12, (config.streamType == StreamType::Main ? "main" : "sub"));
    }

    // 处理认证信息 (user:pass@ip)
    std::string creds = "";
    if (strlen(config.user) > 0) {
        creds = config.user;
        if (strlen(config.pass) > 0) creds += ":" + std::string(config.pass);
        creds += "@";
    }

    // 配置 RTSP 源参数
    ss << "rtspsrc location=\"rtsp://" << creds << config.ip << ":" << config.port << path << "\" "
        << "protocols=" << (config.protocol == TransportProto::TCP ? "tcp" : "udp") << " "
        << "latency=" << config.latency << " " // 延迟缓冲区大小
        << "timeout=" << config.timeout << " "
        << "udp-buffer-size=" << config.udpBufferSize << " "
        << "ntp-sync=" << (config.ntpSync ? "true" : "false") << " "
        << "buffer-mode=" << (int)config.bufferMode << " "
        << "drop-on-latency=" << (config.dropOnLatency ? "true" : "false") << " ! ";

    //解复用与解析
    ss << "rtp" << codecStr << "depay ! " << codecStr << "parse ! ";

    std::string renderFormat = config.useBGRA ? "BGRA" : "RGBA";

    //配置解码器（硬件加速 vs 软件解码）
    if (config.decoder == DecoderType::D3D11_VA) {
            ss << "d3d11" << codecStr << "dec ! d3d11convert ! video/x-raw(memory:D3D11Memory),format=" << renderFormat << " ! d3d11download ! video/x-raw,format=" << renderFormat << " ! ";
    }
    else if (config.decoder == DecoderType::NVIDIA_HW) {
        ss << "nv" << codecStr << "dec ! videoconvert ! video/x-raw,format=" << renderFormat << " ! ";
    }
    else if (config.decoder == DecoderType::INTEL_QSV) {
        ss << "qsv" << codecStr << "dec ! videoconvert ! video/x-raw,format=" << renderFormat << " ! ";
    }
    else {
        // 软件解码使用 libav (ffmpeg)
        ss << "avdec_" << codecStr << " max-threads=" << config.cpuThreads << " ! videoconvert ! video/x-raw,format=" << renderFormat << " ! ";
    }

    //配置输出汇 (appsink)，允许在 C++ 代码中提取每一帧数据
    ss << "appsink name=mysink emit-signals=true "
        << "sync=" << (config.syncToClock ? "true" : "false") << " " // 是否同步时钟（直播通常设为 false 减少延迟）
        << "max-buffers=" << config.maxBuffers << " "
        << "drop=true"; // 缓冲区满时丢弃旧帧

    return ss.str();
}

bool LiveStream::Open(const StreamConfig& config) {
    m_lastErrorMsg.clear();

    std::string fullPipeline = BuildPipelineString(config);
    GError* error = nullptr;

    // 解析字符串创建管道
    m_pipeline = gst_parse_launch(fullPipeline.c_str(), &error);

    if (error || !m_pipeline) {
        // 处理解析错误
        if (error) {
            m_lastErrorMsg = error->message;
            g_error_free(error);
        }

        return false;
    }

    // 获取并连接回调函数，当有新帧到达时调用 OnNewSample
    GstElement* sink = gst_bin_get_by_name(GST_BIN(m_pipeline), "mysink");
    if (sink) {
        g_signal_connect(sink, "new-sample", G_CALLBACK(OnNewSample), this);
        gst_object_unref(sink);
    }

    // 设置管道状态为播放
    GstStateChangeReturn stateRet = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (stateRet == GST_STATE_CHANGE_FAILURE) {
        // 启动失败时的清理逻辑与降级
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;

        if (config.autoHardwareFallback && config.decoder != DecoderType::SOFTWARE) {
            StreamConfig fallbackConfig = config;
            fallbackConfig.decoder = DecoderType::SOFTWARE;
            return Open(fallbackConfig);
        }
        return false;
    }
    return true;
}

GstFlowReturn LiveStream::OnNewSample(GstAppSink* sink, gpointer user_data) {
    auto self = static_cast<LiveStream*>(user_data);
    GstSample* sample = gst_app_sink_pull_sample(sink);
    if (!sample) return GST_FLOW_OK;

    // 获取视频分辨率
    GstCaps* caps = gst_sample_get_caps(sample);
    GstStructure* s = gst_caps_get_structure(caps, 0);
    int width = 0, height = 0;
    gst_structure_get_int(s, "width", &width);
    gst_structure_get_int(s, "height", &height);

    // 映射内存并拷贝数据
    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        // 将数据拷贝到临时缓冲区，以减少锁的占用时间
        std::vector<uint8_t> tmp(map.size);
        memcpy(tmp.data(), map.data, map.size);
        gst_buffer_unmap(buffer, &map);

        // 加锁更新类内部成员
        std::lock_guard<std::mutex> lock(self->m_mutex);
        self->m_width = width;
        self->m_height = height;
        self->m_pixels = std::move(tmp); // 移动语义减少拷贝
        self->m_hasNewFrame = true;
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

void LiveStream::Close() {
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }
    m_image.reset(); // 释放渲染纹理
}

void LiveStream::Update() {
    int w, h;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_hasNewFrame || m_pixels.empty()) return;

        m_localBuffer.swap(m_pixels); // 快速交换缓冲区
        w = m_width;
        h = m_height;
        m_hasNewFrame = false;
    }

    if (w <= 0 || h <= 0) return;

    // 如果图像尺寸变化，重新创建纹理对象
    if (!m_image || m_image->GetWidth() != w || m_image->GetHeight() != h) {
        m_image = std::make_shared<Walnut::Image>(w, h, Walnut::ImageFormat::RGBA);
    }

    // 上传数据到 GPU 纹理 (Walnut 框架封装)
    auto uploadStart = std::chrono::high_resolution_clock::now();
    m_image->SetData(m_localBuffer.data());
    auto uploadEnd = std::chrono::high_resolution_clock::now();

    // 性能监控：如果上传过慢则记录警告
    double uploadMs = std::chrono::duration<double, std::milli>(uploadEnd - uploadStart).count();
    if (uploadMs > 5.0) {
        // OutputDebugStringA 用于在 Windows 调试控制台输出
    }

    // 计算 FPS
    m_frameCount++;
    auto now = std::chrono::high_resolution_clock::now();
    double time = std::chrono::duration<double>(now.time_since_epoch()).count();
    if (time - m_lastFpsTime >= 1.0) {
        m_currentFPS = m_frameCount;
        m_frameCount = 0;
        m_lastFpsTime = time;
    }
}

// 绘制摄像头基础连接信息（IP、端口、账号密码）
void LiveStream::DrawConnectionSettings() {
    if (ImGui::CollapsingHeader("Connection Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("ConnTable", 2, ImGuiTableFlags_SizingStretchProp)) {
            DrawPropertyLabel("Camera Name"); ImGui::InputText("##Name", m_StreamConfig.name, 128);
            DrawPropertyLabel("IP Address");  ImGui::InputText("##IP", m_StreamConfig.ip, 64);
            DrawPropertyLabel("Username");    ImGui::InputText("##User", m_StreamConfig.user, 64);
            DrawPropertyLabel("Password");    ImGui::InputText("##Pass", m_StreamConfig.pass, 64, ImGuiInputTextFlags_Password);
            DrawPropertyLabel("Port");        ImGui::InputInt("##Port", &m_StreamConfig.port);
            ImGui::EndTable();
        }
    }
}

void LiveStream::DrawProtocolCodecSettings() {
    if (ImGui::CollapsingHeader("Protocol & Codec", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("ProtoTable", 2, ImGuiTableFlags_SizingStretchProp)) {

            // --- 品牌选择 ---
            const char* brands[] = { "Hikvision", "Dahua", "Custom" };
            int current_brand = (int)m_StreamConfig.brand;
            DrawPropertyLabel("Camera Brand");
            if (ImGui::Combo("##Brand", &current_brand, brands, IM_ARRAYSIZE(brands)))
                m_StreamConfig.brand = (CameraBrand)current_brand;

            // --- 路径或通道选择 ---
            DrawPropertyLabel(m_StreamConfig.brand == CameraBrand::CUSTOM ? "Custom Path" : "Channel");
            if (m_StreamConfig.brand == CameraBrand::CUSTOM) {
                ImGui::InputText("##CustomPath", m_StreamConfig.customPath, IM_ARRAYSIZE(m_StreamConfig.customPath));
            }
            else {
                ImGui::InputInt("##Channel", &m_StreamConfig.channel);
            }

            // --- 编码格式选择 (H.264 / H.265) ---
            const char* codecs[] = { "H264", "H265" };
            int current_codec = (int)m_StreamConfig.codec;
            DrawPropertyLabel("Codec");
            if (ImGui::Combo("##Codec", &current_codec, codecs, IM_ARRAYSIZE(codecs)))
                m_StreamConfig.codec = (CodecType)current_codec;

            // --- 码流类型选择 (主码流 / 子码流) ---
            const char* streamTypes[] = { "Main Stream", "Sub Stream" };
            int current_stream_type = (int)m_StreamConfig.streamType;
            DrawPropertyLabel("Stream Type");
            if (ImGui::Combo("##StreamType", &current_stream_type, streamTypes, IM_ARRAYSIZE(streamTypes)))
                m_StreamConfig.streamType = (StreamType)current_stream_type;

            // --- 传输协议选择 (TCP / UDP) ---
            const char* protocols[] = { "TCP", "UDP" };
            int current_proto = (int)m_StreamConfig.protocol;
            DrawPropertyLabel("Protocol");
            if (ImGui::Combo("##Protocol", &current_proto, protocols, IM_ARRAYSIZE(protocols)))
                m_StreamConfig.protocol = (TransportProto)current_proto;

            ImGui::EndTable();
        }
    }
}

void LiveStream::DrawNetworkBufferSettings() {
    if (ImGui::CollapsingHeader("Network & Buffer Limits", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("NetBufTable", 2, ImGuiTableFlags_SizingStretchProp)) {

            // --- 延迟时间 (ms) ---
            DrawPropertyLabel("Latency (ms)");
            ImGui::InputInt("##Latency", &m_StreamConfig.latency);

            // --- UDP 接收缓冲区大小 (bytes) ---
            DrawPropertyLabel("UDP Buffer Size");
            ImGui::InputInt("##UDPSize", &m_StreamConfig.udpBufferSize);

            // --- 网络超时时间 (us) ---
            DrawPropertyLabel("Timeout (us)");
            ImGui::InputInt("##Timeout", &m_StreamConfig.timeout);

            // --- 超过延迟阈值是否丢帧 ---
            DrawPropertyLabel("Drop On Latency");
            ImGui::Checkbox("##DropLatency", &m_StreamConfig.dropOnLatency);

            // --- NTP 时间戳同步 ---
            DrawPropertyLabel("NTP Sync");
            ImGui::Checkbox("##NTPSync", &m_StreamConfig.ntpSync);

            // --- 缓冲模式选择 ---
            const char* bufferModes[] = { "Auto", "Slave", "Buffer", "Sync" };
            int current_buf_mode = (int)m_StreamConfig.bufferMode;
            DrawPropertyLabel("Buffer Mode");
            if (ImGui::Combo("##BufferMode", &current_buf_mode, bufferModes, IM_ARRAYSIZE(bufferModes))) {
                m_StreamConfig.bufferMode = (BufferMode)current_buf_mode;
            }

            ImGui::EndTable();
        }
    }
}

// 绘制解码器及渲染配置，并提供“应用并重启”按钮
void LiveStream::DrawDecoderRenderingSettings() {
    if (ImGui::CollapsingHeader("Decoder & Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("DecTable", 2, ImGuiTableFlags_SizingStretchProp)) {
            const char* decoders[] = { "Software (Safe)", "NVIDIA (CUVID)", "DirectX 11 (D3D11)", "Intel QSV" };
            int current_decoder = (int)m_StreamConfig.decoder;
            DrawPropertyLabel("Decoder Type");
            if (ImGui::Combo("##DecoderMode", &current_decoder, decoders, 4))
                m_StreamConfig.decoder = (DecoderType)current_decoder;

            DrawPropertyLabel("Swap Red/Blue");    ImGui::Checkbox("##SwapRB", &m_StreamConfig.useBGRA);

            ImGui::EndTable();

            ImGui::Spacing();
            // 重启按钮：关闭当前管道并应用新配置重新开启
            if (ImGui::Button("Apply & Restart Stream", ImVec2(-1, 0))) {
                Close();
                Open(m_StreamConfig);
            }
        }
    }
}

void LiveStream::DrawStreamConfigPanel() {
    ImGui::PushItemWidth(-1.0f); // 让输入框撑满宽度

    // 显示实时统计数据
    if (ImGui::CollapsingHeader("Live Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Resolution: %dx%d", m_width, m_height);
        ImGui::Text("Frame Rate: %d FPS", m_currentFPS);
    }
    DrawConnectionSettings();
    DrawProtocolCodecSettings();
    DrawNetworkBufferSettings();
    DrawDecoderRenderingSettings();

    ImGui::PopItemWidth();
}