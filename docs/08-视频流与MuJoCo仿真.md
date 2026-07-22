# 08 — 视频流与MuJoCo仿真

> [← 通信协议](07-通信协议与硬件接口.md) | [目录](00-目录与概述.md) | [手柄映射 →](09-手柄映射系统.md)

---

## 9. 视频流与MuJoCo仿真

### 9.1 LiveStream — GStreamer 视频流

**文件**: `Robot-UI/src/LiveStream.h`, `LiveStream.cpp`

基于 **GStreamer** 的 RTSP 视频流解码和渲染：

```cpp
class LiveStream : public EditDraftBase {
public:
    bool Open(const StreamConfig& config);
    void Close();
    void Update();
    void* GetDescriptorSet() const;  // Vulkan Descriptor Set
    bool IsReady() const;
    int GetWidth() const;
    int GetHeight() const;
    int GetFPS() const;
};
```

#### StreamConfig 配置项

```cpp
struct StreamConfig {
    char name[128];
    char ip[64], user[64], pass[64];
    int  port;
    CameraBrand brand;           // HIKVISION, DAHUA, CUSTOM
    int  channel;
    CodecType codec;             // H264, H265, H265_PLUS
    StreamType streamType;       // Main, Sub
    TransportProto protocol;     // TCP, UDP
    char customPath[256];
    int  latency, udpBufferSize, timeout;
    bool dropOnLatency, ntpSync;
    BufferMode bufferMode;       // AUTO, SLAVE, BUFFER, SYNC
    DecoderType decoder;         // SOFTWARE, NVIDIA_HW, D3D11_VA, INTEL_QSV
    int  cpuThreads;
    bool syncToClock, lowLatencyMode, useBGRA, autoHardwareFallback;
};
```

#### 解码器支持

| 解码器 | 说明 |
|--------|------|
| **SOFTWARE** | CPU 软解 (FFmpeg libav) |
| **NVIDIA_HW** | NVIDIA NVDEC 硬解 |
| **D3D11_VA** | Windows D3D11 Video Acceleration |
| **INTEL_QSV** | Intel QuickSync 硬解 |

### 9.2 LiveStreamManager — 视频流管理器

**文件**: `Robot-UI/src/LiveStreamManager.h`

```cpp
class LiveStreamManager : public ManagerBase {
    std::vector<DeviceNode> m_devices;  // 多设备管理
};
```

### 9.3 MuJoCo 仿真线程

**文件**: `Robot-UI/src/mujoco_thread/mujoco_thread.h`, `mujoco_thread.cpp`

MuJoCo 物理仿真的 C++ 封装：

```cpp
namespace mujoco {
    class mujoco_thread {
    public:
        mujoco_thread(std::string model_file, double max_FPS = 60,
                      int width = 1200, int height = 900,
                      std::string title = "MUJOCO");
        void load_model(std::string model_file);
        void sim();                    // 主仿真循环
        virtual void step() = 0;      // 纯虚拟，子类实现
        virtual void draw() = 0;      // 纯虚拟，子类实现
        void render();                // OpenGL 渲染
        void close_render();

    protected:
        mjModel* m;    // MuJoCo model
        mjData*  d;    // MuJoCo data
        mjvCamera cam;
        mjvOption opt;
        mjvScene scn;
        mjrContext con;
    };
}
```

### 9.4 水下机器人仿真

**文件**: `Robot-UI/src/Robot_API/ROV_API/UnderwaterRobot_Sim.h`, `UnderwaterRobot_Sim.cpp`

```cpp
namespace mujoco {
    enum MotorID {
        LEFT_HIP, RIGHT_HIP,       // 髋关节
        LEFT_THIGH, RIGHT_THIGH,   // 大腿
        LEFT_CALF, RIGHT_CALF,     // 小腿
        LEFT_WHEEL, RIGHT_WHEEL    // 轮子
    };

    class mj_env : public mujoco_thread {
        MotorConfig motors[8];
        int joint_pos_id[8], joint_vel_id[8];
        mjtNum actual_torques[8];
        int base_ori_id, base_angvel_id, base_linacc_id;
        int base_linvel_id, base_pos_id;
    };
}
```

仿真机器人是一个 **8 自由度轮腿机器人**：

```
关节布局:
  髋关节(2) ──→ 大腿(2) ──→ 小腿(2)
  轮子(2)  直接驱动
```

---

> [← 通信协议](07-通信协议与硬件接口.md) | [目录](00-目录与概述.md) | [手柄映射 →](09-手柄映射系统.md)
