#include "LiveStream.h"

// ======== 辅助函数 ========
static void DrawPropertyLabel(const char* label) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label); //
    ImGui::TableNextColumn();      //
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

// 返回用于 RTSP URL 路径的 codec 字符串
static std::string GetCodecUrlStr(const StreamConfig& config) {
    switch (config.codec) {
        case CodecType::H265:       return "h265";
        case CodecType::H265_PLUS:  return "h265+";
        default:                    return "h264";
    }
}

// 返回用于 GStreamer 管线元素的 codec 字符串（H.265+ 解码器与 H.265 相同）
static std::string GetCodecPipelineStr(const StreamConfig& config) {
    switch (config.codec) {
        case CodecType::H265:
        case CodecType::H265_PLUS:  return "h265";
        default:                    return "h264";
    }
}

// ======== GStreamer 管线构建 ========

// 构建 rtspsrc 的 RTSP URL 路径
static std::string BuildRTSPSrcPath(const StreamConfig& config, const std::string& codecStr) {
    if (config.brand == CameraBrand::HIKVISION) {
        return "/" + codecStr + "/ch" + std::to_string(config.channel) + "/" +
            (config.streamType == StreamType::Main ? "main" : "sub") + "/av_stream";
    }
    else if (config.brand == CameraBrand::DAHUA) {
        return "/cam/realmonitor?channel=" + std::to_string(config.channel) +
            "&subtype=" + (config.streamType == StreamType::Main ? "0" : "1");
    }
    else {
        std::string path = config.customPath;
        size_t pos;
        while ((pos = path.find("{codec}")) != std::string::npos) path.replace(pos, 7, codecStr);
        while ((pos = path.find("{channel}")) != std::string::npos) path.replace(pos, 9, std::to_string(config.channel));
        while ((pos = path.find("{streamType}")) != std::string::npos) path.replace(pos, 12, (config.streamType == StreamType::Main ? "main" : "sub"));
        return path;
    }
}

// 构建 rtspsrc 的属性字符串（仅包含非默认值的参数）
static std::string BuildRTSPSrcProperties(const StreamConfig& config) {
    std::stringstream ss;

    // 基础属性 — 始终输出
    ss << "protocols=" << (config.protocol == TransportProto::TCP ? "tcp" : "udp");
    ss << " latency=" << config.latency;

    // 可选属性 — 仅在非默认值时输出，让 GStreamer 使用自身默认值
    if (config.bufferMode != BufferMode::AUTO)
        ss << " buffer-mode=" << (int)config.bufferMode;
    if (config.dropOnLatency)
        ss << " drop-on-latency=true";
    if (config.timeout != 5000000)
        ss << " timeout=" << config.timeout;
    if (config.udpBufferSize > 0)
        ss << " udp-buffer-size=" << config.udpBufferSize;
    if (config.ntpSync)
        ss << " ntp-sync=true";

    return ss.str();
}

// 构建解码+转换管线（appsink 路径）
static std::string BuildDecoderPipeline(const StreamConfig& config, const std::string& codecStr) {
    std::stringstream ss;
    std::string renderFormat = config.useBGRA ? "BGRA" : "RGBA";

    ss << "rtp" << codecStr << "depay ! " << codecStr << "parse ! ";

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
        ss << "avdec_" << codecStr << " max-threads=" << config.cpuThreads << " ! videoconvert ! video/x-raw,format=" << renderFormat << " ! ";
    }

    // appsink — 将帧传入 CPU 内存供 ImGui 渲染
    ss << "appsink name=mysink emit-signals=true "
        << "sync=" << (config.syncToClock ? "true" : "false") << " "
        << "max-buffers=" << config.maxBuffers << " "
        << "drop=true";
    return ss.str();
}

// 构建直接 GPU 渲染管线（d3d11videosink，用于 CLI 参考）
static std::string BuildDirectRenderPipeline(const StreamConfig& config, const std::string& codecStr) {
    std::stringstream ss;
    ss << "rtp" << codecStr << "depay ! " << codecStr << "parse ! ";

    if (config.decoder == DecoderType::D3D11_VA) {
        ss << "d3d11" << codecStr << "dec ! d3d11videosink sync=false";
    }
    else if (config.decoder == DecoderType::NVIDIA_HW) {
        ss << "nv" << codecStr << "dec ! d3d11videosink sync=false";
    }
    else if (config.decoder == DecoderType::INTEL_QSV) {
        ss << "qsv" << codecStr << "dec ! d3d11videosink sync=false";
    }
    else {
        ss << "avdec_" << codecStr << " ! d3d11videosink sync=false";
    }
    return ss.str();
}

std::string LiveStream::BuildPipelineString(const StreamConfig& config) {
    std::string urlCodecStr    = GetCodecUrlStr(config);
    std::string pipelineStr    = GetCodecPipelineStr(config);
    std::string path = BuildRTSPSrcPath(config, urlCodecStr);

    // 认证信息 (user:pass@)
    std::string creds;
    if (strlen(config.user) > 0) {
        creds = config.user;
        if (strlen(config.pass) > 0) creds += ":" + std::string(config.pass);
        creds += "@";
    }

    std::stringstream ss;
    ss << "rtspsrc location=\"rtsp://" << creds << config.ip << ":" << config.port << path << "\" ";
    ss << BuildRTSPSrcProperties(config);
    ss << " ! ";
    ss << BuildDecoderPipeline(config, pipelineStr);
    return ss.str();
}

// 生成 CLI 参考命令（使用 d3d11videosink 直接渲染，最低延迟）
std::string LiveStream::BuildCLIReferenceString(const StreamConfig& config) {
    std::string urlCodecStr    = GetCodecUrlStr(config);
    std::string pipelineStr    = GetCodecPipelineStr(config);
    std::string path = BuildRTSPSrcPath(config, urlCodecStr);

    // 认证信息
    std::string creds;
    if (strlen(config.user) > 0) {
        creds = config.user;
        if (strlen(config.pass) > 0) creds += ":" + std::string(config.pass);
        creds += "@";
    }

    std::stringstream ss;
    ss << "gst-launch-1.0.exe -v rtspsrc location=\"rtsp://" << creds << config.ip << ":" << config.port << path << "\" ";
    ss << BuildRTSPSrcProperties(config);
    ss << " ! ";
    ss << BuildDirectRenderPipeline(config, pipelineStr);
    return ss.str();
}

bool LiveStream::Open(const StreamConfig& config) {
    m_lastErrorMsg.clear();
    std::string fullPipeline = BuildPipelineString(config);
    GError* error = nullptr;
    m_pipeline = gst_parse_launch(fullPipeline.c_str(), &error);
    if (error || !m_pipeline) {
        //
        if (error) {
            m_lastErrorMsg = error->message;
            g_error_free(error);
        }
        return false;
    }
    //  OnNewSample
    GstElement* sink = gst_bin_get_by_name(GST_BIN(m_pipeline), "mysink");
    if (sink) {
        g_signal_connect(sink, "new-sample", G_CALLBACK(OnNewSample), this);
        gst_object_unref(sink);
    }
    //
    GstStateChangeReturn stateRet = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);

    if (stateRet == GST_STATE_CHANGE_FAILURE) {
        //
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
    //
    GstCaps* caps = gst_sample_get_caps(sample);
    GstStructure* s = gst_caps_get_structure(caps, 0);
    int width = 0, height = 0;
    gst_structure_get_int(s, "width", &width);
    gst_structure_get_int(s, "height", &height);
    //
    GstBuffer* buffer = gst_sample_get_buffer(sample);
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
    if (m_pipeline) {
        // 先发信号让 pipeline 停止接收新 buffer
        GstElement* sink = gst_bin_get_by_name(GST_BIN(m_pipeline), "mysink");
        if (sink) {
            g_signal_handlers_disconnect_by_data(sink, this);
            gst_object_unref(sink);
        }
        // 设置 NULL 状态并等待管线完全停止
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        GstState state, pending;
        gst_element_get_state(m_pipeline, &state, &pending, GST_CLOCK_TIME_NONE);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }
    m_image.reset();
}

void LiveStream::Update() {
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
// IP
void LiveStream::DrawConnectionSettings() {
    if (ImGui::CollapsingHeader("Connection Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("ConnTable", 2, ImGuiTableFlags_SizingStretchProp)) {
            DrawPropertyLabel("IP Address");  ImGui::InputText("##IP", m_StreamConfig.ip, IM_ARRAYSIZE(m_StreamConfig.ip));
            DrawPropertyLabel("Username");    ImGui::InputText("##User", m_StreamConfig.user, IM_ARRAYSIZE(m_StreamConfig.user));
            DrawPropertyLabel("Password");    ImGui::InputText("##Pass", m_StreamConfig.pass, IM_ARRAYSIZE(m_StreamConfig.pass), ImGuiInputTextFlags_Password);
            DrawPropertyLabel("Port");        ImGui::InputInt("##Port", &m_StreamConfig.port);
            ImGui::EndTable();
        }
    }
}

void LiveStream::DrawProtocolCodecSettings() {
    if (ImGui::CollapsingHeader("Protocol & Codec", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("ProtoTable", 2, ImGuiTableFlags_SizingStretchProp)) {
            // ---  ---
            const char* brands[] = { "Hikvision", "Dahua", "Custom" };
            int current_brand = (int)m_StreamConfig.brand;
            DrawPropertyLabel("Camera Brand");
            if (ImGui::Combo("##Brand", &current_brand, brands, IM_ARRAYSIZE(brands)))
                m_StreamConfig.brand = (CameraBrand)current_brand;
            // ---  ---
            DrawPropertyLabel(m_StreamConfig.brand == CameraBrand::CUSTOM ? "Custom Path" : "Channel");
            if (m_StreamConfig.brand == CameraBrand::CUSTOM) {
                ImGui::InputText("##CustomPath", m_StreamConfig.customPath, IM_ARRAYSIZE(m_StreamConfig.customPath));
            }
            else {
                ImGui::InputInt("##Channel", &m_StreamConfig.channel);
            }
            // ---  (H.264 / H.265 / H.265+) ---
            const char* codecs[] = { "H264", "H265", "H.265+" };
            int current_codec = (int)m_StreamConfig.codec;
            DrawPropertyLabel("Codec");
            if (ImGui::Combo("##Codec", &current_codec, codecs, IM_ARRAYSIZE(codecs)))
                m_StreamConfig.codec = (CodecType)current_codec;
            // ---  ( / ) ---
            const char* streamTypes[] = { "Main Stream", "Sub Stream" };
            int current_stream_type = (int)m_StreamConfig.streamType;
            DrawPropertyLabel("Stream Type");
            if (ImGui::Combo("##StreamType", &current_stream_type, streamTypes, IM_ARRAYSIZE(streamTypes)))
                m_StreamConfig.streamType = (StreamType)current_stream_type;
            // ---  (TCP / UDP) ---
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
            // ---  (ms) ---
            DrawPropertyLabel("Latency (ms)");
            ImGui::InputInt("##Latency", &m_StreamConfig.latency);
            // --- UDP  (bytes) ---
            DrawPropertyLabel("UDP Buffer Size");
            ImGui::InputInt("##UDPSize", &m_StreamConfig.udpBufferSize);
            // ---  (us) ---
            DrawPropertyLabel("Timeout (us)");
            ImGui::InputInt("##Timeout", &m_StreamConfig.timeout);
            // ---  ---
            DrawPropertyLabel("Drop On Latency");
            ImGui::Checkbox("##DropLatency", &m_StreamConfig.dropOnLatency);
            // --- NTP  ---
            DrawPropertyLabel("NTP Sync");
            ImGui::Checkbox("##NTPSync", &m_StreamConfig.ntpSync);
            // ---  ---
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
//
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
        }
    }
}

void LiveStream::DrawNoticePanel() {
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
    if (ImGui::CollapsingHeader("Notice", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed)) {
        ImGui::PopStyleColor(3);
        ImGui::TextWrapped("Performance Benchmark:");
        ImGui::BulletText("CLI (Direct GPU Overlay): ~100ms latency.");
        ImGui::BulletText("Software (AppSink + Texture Upload): ~200ms latency.");
        ImGui::Spacing();
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().WindowPadding.x);
        ImGui::TextDisabled("Note: The 100ms difference is the physical overhead of copying frames "
            "from Video Memory back to System RAM for UI synchronization.");
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("Reference CLI Command (Minimum Latency):");
        std::string cliCmd = BuildCLIReferenceString(m_StreamConfig);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
        ImVec2 textSize = ImGui::CalcTextSize(cliCmd.c_str(), nullptr, false, ImGui::GetContentRegionAvail().x - 20.0f);
        float childHeight = textSize.y + ImGui::GetStyle().FramePadding.y * 4.0f + 15.0f;
        if (ImGui::BeginChild("##CLI_Box", ImVec2(-1.0f, childHeight), true, ImGuiWindowFlags_NoScrollbar)) {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(cliCmd.c_str());
            ImGui::PopTextWrapPos();
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0))
                ImGui::SetClipboardText(cliCmd.c_str());
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Click to copy text");
    } else {
        ImGui::PopStyleColor(3);
    }
}

void LiveStream::DrawStreamConfigPanel() {
    ImGui::PushItemWidth(-1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    DrawNoticePanel();
    ImGui::Spacing();
    DrawConnectionSettings();
    DrawProtocolCodecSettings();
    DrawNetworkBufferSettings();
    DrawDecoderRenderingSettings();
    ImGui::PopStyleVar();
    ImGui::PopItemWidth();
}
