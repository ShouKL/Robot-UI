# Robot-UI App Template

This is a Robot-UI app template for [Walnut](https://github.com/TheCherno/Walnut). See the Walnut repository for more details.

## Getting Started

```shell
$ git clone --recursive https://github.com/ShouKL/Robot-UI.git 
```

Once you've cloned, you need Install required dependencies. 
| dependency  | link      | Env Variables      | Value |
|-------------|-----------|-----------------|------------------|
| OpenCV    | https://opencv.org/releases/ Windows| OPENCV | ...\opencv\build |
| gstreamer | https://gstreamer.freedesktop.org/download/#windows MSVC x86_64 (VS 2022, Release CRT)| GSTREAMER | ...\gstreamer\1.0\msvc_x86_64 |
| Vulkan | https://vulkan.lunarg.com/sdk/home vulkansdk-windows-X64-1.4.341.1.exe | VULKAN_SDK | ...\VulkanSDK\1.4.341.1 |
| Mujoco | https://github.com/google-deepmind/mujoco/releases mujoco-3.8.0-windows-x86_64.zip | MUJOCO | ...\mujoco |

Once you're happy, run `scripts/Setup.bat` to generate Visual Studio 2026 solution/project files. If you want to generate vs2022, you can replace vs2026 with vs2022 in `scripts/Setup.bat`. I recommend modifying that Robot-UI APP project to create your own application, as everything should be setup and ready to go.