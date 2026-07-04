#include "Screenshot.h"

#include "Walnut/Application.h"
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <imgui_internal.h>
#include <chrono>
#include <ctime>
#include <fstream>

static const char* s_WindowNames[] = {
    "Main Client",          // 0
    "Full Window",          // 1
    "Entire Screen",        // 2
    "Option",               // 3
    "Robot Status",         // 4
    "Monitor Wall",         // 5
    "Terminal",             // 6
    "Thrust Curve Editor",  // 7
    "Robot Setting",        // 8
    "About",                // 9
};

const char** Screenshot::GetWindowNames()
{
    return s_WindowNames;
}

// ---- 写 BMP 文件（24-bit）----
bool Screenshot::WriteBMP24(const std::string& filepath,
                             int width, int height,
                             const std::vector<uint8_t>& pixels24)
{
    std::ofstream ofs(filepath, std::ios::binary);
    if (!ofs.is_open()) return false;

    int rowSize = ((width * 3 + 3) / 4) * 4;
    BITMAPFILEHEADER bfh = {};
    bfh.bfType    = 0x4D42;
    bfh.bfSize    = sizeof(bfh) + sizeof(BITMAPINFOHEADER) + rowSize * height;
    bfh.bfOffBits = sizeof(bfh) + sizeof(BITMAPINFOHEADER);

    BITMAPINFOHEADER bih = {};
    bih.biSize        = sizeof(bih);
    bih.biWidth       = width;
    bih.biHeight      = height;
    bih.biPlanes      = 1;
    bih.biBitCount    = 24;
    bih.biCompression = BI_RGB;

    ofs.write(reinterpret_cast<const char*>(&bfh), sizeof(bfh));
    ofs.write(reinterpret_cast<const char*>(&bih), sizeof(bih));
    ofs.write(reinterpret_cast<const char*>(pixels24.data()), rowSize * height);
    ofs.close();
    return true;
}

// ---- 从屏幕 DC BitBlt 指定矩形 ----
bool Screenshot::CaptureScreenRect(void* pRect, int& outW, int& outH,
                                    std::vector<uint8_t>& outPixels)
{
    RECT& rect = *static_cast<RECT*>(pRect);
    outW = rect.right - rect.left;
    outH = rect.bottom - rect.top;
    if (outW <= 0 || outH <= 0) return false;

    HDC hScr = GetDC(nullptr);
    HDC hMem = CreateCompatibleDC(hScr);
    HBITMAP hBmp = CreateCompatibleBitmap(hScr, outW, outH);
    HBITMAP hOld = (HBITMAP)SelectObject(hMem, hBmp);

    bool ok = BitBlt(hMem, 0, 0, outW, outH, hScr, rect.left, rect.top, SRCCOPY) != 0;
    if (ok)
    {
        int rowSize = ((outW * 3 + 3) / 4) * 4;
        outPixels.resize(rowSize * outH);

        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth  = outW;
        bi.bmiHeader.biHeight = outH;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 24;
        bi.bmiHeader.biCompression = BI_RGB;
        GetDIBits(hMem, hBmp, 0, outH, outPixels.data(), &bi, DIB_RGB_COLORS);
    }

    SelectObject(hMem, hOld);
    DeleteObject(hBmp);
    DeleteDC(hMem);
    ReleaseDC(nullptr, hScr);
    return ok;
}

// ---- 主入口：截图并保存到指定文件 ----
bool Screenshot::Capture(int scope,
                          const std::string& saveDir,
                          const std::string& outFilename)
{
    GLFWwindow* glfwWin = Walnut::Application::Get().GetWindowHandle();
    HWND hwnd = glfwWin ? glfwGetWin32Window(glfwWin) : nullptr;

    RECT rect = {};

    if (scope >= OptionPanel && scope < COUNT)
    {
        ImGuiWindow* win = ImGui::FindWindowByName(s_WindowNames[scope]);
        if (!win)
        {
            WL_ERROR_TAG("APP", "Screenshot: ImGui window '{}' not found", s_WindowNames[scope]);
            return false;
        }
        rect.left   = (LONG)win->Pos.x;
        rect.top    = (LONG)win->Pos.y;
        rect.right  = (LONG)(win->Pos.x + win->Size.x);
        rect.bottom = (LONG)(win->Pos.y + win->Size.y);
    }
    else if (scope == MainClient && hwnd)
    {
        GetClientRect(hwnd, &rect);
        POINT pt = {0, 0};
        ClientToScreen(hwnd, &pt);
        rect.left   = pt.x;
        rect.top    = pt.y;
        rect.right  = pt.x + rect.right;
        rect.bottom = pt.y + rect.bottom;
    }
    else if (scope == FullWindow && hwnd)
    {
        GetWindowRect(hwnd, &rect);
    }
    else
    {
        rect.left   = GetSystemMetrics(SM_XVIRTUALSCREEN);
        rect.top    = GetSystemMetrics(SM_YVIRTUALSCREEN);
        rect.right  = rect.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
        rect.bottom = rect.top  + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    }

    int capW, capH;
    std::vector<uint8_t> pixels;
    if (!CaptureScreenRect(&rect, capW, capH, pixels))
        return false;

    return WriteBMP24(outFilename, capW, capH, pixels);
}
