#include "StreamManager.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include "imgui.h"
#include <vector>
#include <memory>

StreamManager::StreamManager() {
    AddDevice("Default_Hikvision", "192.168.0.123"); // 使用海康威视作为默认预设
}

StreamManager::~StreamManager() {
        for (auto& node : m_devices) {
            node.stream->Close();                         // 循环关闭所有活动的流
        }
        m_devices.clear();                                // 清空容器，触发 unique_ptr 自动释放
}

void StreamManager::AddDevice(const char* name, const char* ip) {
    DeviceNode node;                                 // 创建一个新的设备节点
    node.id = m_nextId++;                            // 分配唯一 ID 并递增计数器
    node.stream = std::make_unique<LiveStream>();    // 为该节点创建唯一的流处理实例

    auto& config = node.stream->GetStreamConfig();   // 获取该流的配置引用进行初始化

    // 安全地复制设备名称，确保以空字符结尾防止溢出
    strncpy(config.name, name, sizeof(config.name) - 1);
    config.name[sizeof(config.name) - 1] = '\0';

    // 安全地复制 IP 地址
    strncpy(config.ip, ip, sizeof(config.ip) - 1);
    config.ip[sizeof(config.ip) - 1] = '\0';

    // 设置默认的 GStreamer/流媒体参数
    config.latency = 0;                               // 设置为 0 延迟模式
    config.udpBufferSize = 8388608;                   // 设置 UDP 缓存为 8MB (高码率防止丢包)
    config.protocol = TransportProto::UDP;            // 默认使用 UDP 协议
    config.bufferMode = BufferMode::AUTO;             // 自动缓冲模式
    config.dropOnLatency = true;                      // 延迟过高时丢弃过期帧
    config.syncToClock = false;                       // 不强制同步系统时钟（减少卡顿）
    config.ntpSync = false;                           // 禁用 NTP 网络对时同步
    config.decoder = DecoderType::NVIDIA_HW;          // 默认开启 NVIDIA 硬件加速解码

    m_devices.push_back(std::move(node));             // 将配置好的节点移动存储到容器中
}

void StreamManager::RemoveDevice(int id) {
    // 在容器中查找 ID 匹配的设备
    auto it = std::find_if(m_devices.begin(), m_devices.end(), [id](const DeviceNode& n) {
        return n.id == id;
        });

    if (it != m_devices.end()) {                      // 如果找到了该设备
        if (it->isStreaming) {                        // 如果正在播放
            it->stream->Close();                      // 先关闭视频流
        }
        m_devices.erase(it);                          // 从设备列表中移除
    }
}

void StreamManager::UpdateAll() {
    for (auto& node : m_devices) {                    // 遍历所有设备
        if (node.isStreaming) {                       // 只有正在运行的设备才执行更新
            node.stream->Update();                    // 处理帧拷贝、纹理上传等逻辑
        }
    }
}

void StreamManager::DrawUI(const char* windowName, bool* p_open) {
    // 渲染“设备列表”主窗口，带菜单栏
    ImGui::Begin(windowName, p_open, ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar()) {                      // 开启菜单栏
        if (ImGui::BeginMenu("DEVICES LIST")) {       // 创建菜单项
            if (ImGui::MenuItem("Add New Device"))
                AddDevice("New_Camera", "0.0.0.0");   // 点击则新增一个空白设备
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    DrawDeviceTree();                                 // 渲染左侧设备树/列表
    ImGui::End();

    // 渲染“设备配置”右侧窗口
    ImGui::Begin("Device Configuration", p_open);
    DrawControlPanel();                               // 渲染具体的参数控制面板
    ImGui::End();

    DrawMonitorWall();                                // 渲染所有已打开的视频监控小窗口
}

void StreamManager::DrawDeviceTree() {
    int nodeToDelete = -1; // 暂存待删除项 ID，避免在遍历 vector 时删除导致崩溃

    for (auto& node : m_devices) {
        char label[128];
        // 拼接显示名称，## 后面是隐藏的 ID，用于区分同名设备
        snprintf(label, sizeof(label), "%s##%d", node.stream->GetStreamConfig().name, node.id);

        // 渲染一个可点击的选择项
        if (ImGui::Selectable(label, node.isSelected)) {
            for (auto& n : m_devices) n.isSelected = false; // 先取消其他所有设备的选择
            node.isSelected = true;                        // 设当前为选中
        }

        // 逻辑：右键点击设备项时弹出快捷菜单
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete Device")) {
                nodeToDelete = node.id;                // 标记该 ID 待删除
            }
            ImGui::EndPopup();
        }

        // 在同一行末尾显示 ON/OFF 状态灯
        ImGui::SameLine(ImGui::GetColumnWidth() - 30);
        if (node.isStreaming)
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "ON");  // 绿色表示在线
        else
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "OFF"); // 红色表示断开
    }

    // 在循环结束后安全地执行删除
    if (nodeToDelete != -1) {
        RemoveDevice(nodeToDelete);
    }
}

void StreamManager::DrawControlPanel() {
    DeviceNode* selectedNode = nullptr;
    // 寻找当前被选中的设备
    for (auto& n : m_devices) if (n.isSelected) selectedNode = &n;

    if (!selectedNode) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Select a device from the list to configure.");
        return;
    }

    auto& cfg = selectedNode->stream->GetStreamConfig(); // 获取选中设备的配置引用

    ImGui::Separator();

    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%s", cfg.name); // 显示设备名
    ImGui::TextDisabled("ID: %d | Status: %s", selectedNode->id, selectedNode->isStreaming ? "CONNECTED" : "OFFLINE");
    ImGui::Spacing();

    // 计算底部按钮预留的高度，实现局部滚动
    float bottomReservedHeight = ImGui::GetFrameHeightWithSpacing() * 1.5f + ImGui::GetStyle().ItemSpacing.y * 3.0f;
    float scrollWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x;

    // 开启一个子滚动区域
    ImGui::BeginChild("ConfigScroll", ImVec2(scrollWidth, -bottomReservedHeight), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));

    // Lambda 表达式：渲染统一样式的折叠页眉
    auto drawHeader = [](const char* title) {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
        bool open = ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed);
        ImGui::PopStyleColor(3);
        return open;
        };

    // 渲染提示说明板块
    if (drawHeader("Notice")) {
        ImGui::TextWrapped("Performance Benchmark:");
        ImGui::BulletText("CLI (Direct GPU Overlay): ~100ms latency."); // 性能提示
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

        // 拼接一段 GStreamer 命令行字符串供用户参考
        const char* cliCmd = "gst-launch-1.0 -v rtspsrc location=\"rtsp://{user}:{password}@{ip}:554/h264/ch1/main/av_stream\" "
            "latency=0 buffer-mode=0 drop-on-latency=true protocols=udp ! rtph264depay ! h264parse ! "
            "d3d11h264dec ! d3d11videosink sync=false";

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));

        // 计算文本高度并渲染一个可点击复制的代码框
        ImVec2 textSize = ImGui::CalcTextSize(cliCmd, nullptr, false, ImGui::GetContentRegionAvail().x - 20.0f);
        float childHeight = textSize.y + ImGui::GetStyle().FramePadding.y * 4.0f + 15.0f;

        if (ImGui::BeginChild("##CLI_Box", ImVec2(-1.0f, childHeight), true, ImGuiWindowFlags_NoScrollbar)) {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(cliCmd);
            ImGui::PopTextWrapPos();
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
                ImGui::SetClipboardText(cliCmd); // 点击即可复制代码到系统剪贴板
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Click to copy text");
        }
    }

    ImGui::Spacing();

    // 委派给 LiveStream 实例去渲染它自己的参数调节控件（如 IP, Port, 码率等）
    selectedNode->stream->DrawStreamConfigPanel();

    ImGui::PopStyleVar();
    ImGui::EndChild(); // 结束滚动区域

    ImGui::Spacing();

    // 渲染底部的连接/断开按钮
    ImVec2 btnSize(-1.0f, ImGui::GetFrameHeightWithSpacing() * 1.5f);
    if (selectedNode->isStreaming) {
        // 如果在线，显示红色“断开”按钮
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        if (ImGui::Button("DISCONNECT STREAM", btnSize)) {
            selectedNode->stream->Close();           // 关闭流
            selectedNode->isStreaming = false;       // 更新状态
        }
        ImGui::PopStyleColor(3);
    }
    else {
        // 如果离线，显示绿色“连接”按钮
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.8f, 0.5f, 1.0f));
        if (ImGui::Button("APPLY & CONNECT", btnSize)) {
            if (selectedNode->stream->Open(cfg)) {   // 尝试根据当前配置打开流
                selectedNode->isStreaming = true;    // 成功则设为在线
            }
        }
        ImGui::PopStyleColor(3);
    }
}

// 渲染视频监控墙（每个视频一个独立窗口）
void StreamManager::DrawMonitorWall() {
    for (auto& node : m_devices) {                    // 遍历设备
        if (node.isStreaming) {                       // 只有在线的才显示窗口
            char winName[128];
            // 拼接窗口标题，### 后面是唯一标识符，保证同名摄像头不会窗口冲突
            snprintf(winName, sizeof(winName), "%s (ID: %d)###Camera_%d", node.stream->GetStreamConfig().name, node.id, node.id);

            bool isOpen = true;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f)); // 视频窗口不留白边
            if (ImGui::Begin(winName, &isOpen)) {
                if (node.stream->IsReady()) {         // 如果流已经成功解码并有了第一帧纹理
                    // 将底层 API 的纹理句柄转换为 ImGui 可用的 ID
                    ImTextureID texID = (ImTextureID)node.stream->GetDescriptorSet();
                    ImVec2 avail = ImGui::GetContentRegionAvail(); // 获取窗口可用大小

                    // 计算 16:9 的等比例缩放大小
                    float aspect = 16.0f / 9.0f;
                    float targetW = avail.x;
                    float targetH = targetW / aspect;

                    if (targetH > avail.y) {          // 如果高度超限，则以高度为基准缩放
                        targetH = avail.y;
                        targetW = targetH * aspect;
                    }

                    // 居中显示视频画面
                    ImVec2 curPos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(curPos.x + (avail.x - targetW) * 0.5f, curPos.y + (avail.y - targetH) * 0.5f));

                    // 渲染纹理图片
                    ImGui::Image(texID, ImVec2(targetW, targetH));
                }
                else {
                    // 如果正在连接中，显示加载提示
                    ImVec2 avail = ImGui::GetContentRegionAvail();
                    const char* text = "Connecting & Buffering...";
                    ImVec2 textSize = ImGui::CalcTextSize(text);
                    ImGui::SetCursorPos(ImVec2((avail.x - textSize.x) * 0.5f, (avail.y - textSize.y) * 0.5f));
                    ImGui::TextDisabled("%s", text);
                }
            }
            ImGui::End();                             // 结束当前监控窗口
            ImGui::PopStyleVar();                     // 恢复边距样式

            // 如果用户点击了窗口右上角的 X，关闭流
            if (!isOpen) {
                node.stream->Close();
                node.isStreaming = false;
            }
        }
    }
}
