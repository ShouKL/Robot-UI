#pragma once

#include <string>
#include <vector>

// ============================================================================
// Screenshot — 截图工具
// ============================================================================

class Screenshot
{
public:
    // 范围索引（与 OptionPanel scopeItems 顺序一致）
    enum Scope : int {
        MainClient    = 0,
        FullWindow    = 1,
        EntireScreen  = 2,
        OptionPanel   = 3,
        RobotStatus   = 4,
        MonitorWall   = 5,
        Terminal      = 6,
        ThrustCurve   = 7,
        RobotSetting  = 8,
        About         = 9,
        COUNT
    };

    static const char** GetWindowNames();

    // 执行截图：scope → 文件
    static bool Capture(int scope,
                        const std::string& saveDir,
                        const std::string& outFilename);

private:
    static bool WriteBMP24(const std::string& filepath,
                           int width, int height,
                           const std::vector<uint8_t>& pixels24);

    // rect 为 RECT*，用 void* 规避头文件引入
    static bool CaptureScreenRect(void* rect, int& outW, int& outH,
                                  std::vector<uint8_t>& outPixels);
};
