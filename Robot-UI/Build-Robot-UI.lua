project "Robot-UI"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"
   targetdir "bin/%{cfg.buildcfg}"
   staticruntime "off"

   files { "src/**.h", "src/**.cpp",
           "../vendor/imgui-node-editor/crude_json.cpp",
           "../vendor/imgui-node-editor/imgui_canvas.cpp",
           "../vendor/imgui-node-editor/imgui_node_editor.cpp",
           "../vendor/imgui-node-editor/imgui_node_editor_api.cpp",
           "../vendor/implot/implot.cpp",
           "../vendor/implot/implot_items.cpp" }


   OPENCV = os.getenv("OPENCV")
   MUJOCO = os.getenv("MUJOCO")
   GSTREAMER = os.getenv("GSTREAMER")

   IncludeDir = IncludeDir or {}
   IncludeDir["OpenCV"] = "%{OPENCV}/include"
   IncludeDir["Mujoco"] = "%{MUJOCO}/include"
   IncludeDir["GStreamer"] = "%{GSTREAMER}/include"
   IncludeDir["NodeEditor"] = "../vendor/imgui-node-editor"
   IncludeDir["ImPlot"] = "../vendor/implot"
   
   LibraryDir = LibraryDir or {}
   LibraryDir["OpenCV"] = "%{OPENCV}/x64/vc16/lib"
   LibraryDir["Mujoco"] = "%{MUJOCO}/lib"
   LibraryDir["GStreamer"] = "%{GSTREAMER}/lib"

   includedirs
   {
      "../Walnut/vendor/imgui",
      "../Walnut/vendor/glfw/include",
      "../Walnut/vendor/glm",
      "../Walnut/vendor/yaml-cpp/include",
      "../Walnut/vendor/spdlog/include",

      "../Walnut/Walnut/Platform/GUI",
      "../Walnut/Walnut/Source",

      "../vendor/imgui-style",

      "%{IncludeDir.VulkanSDK}",
      "%{IncludeDir.OpenCV}",
      "%{IncludeDir.Mujoco}",
      "%{IncludeDir.GStreamer}/gstreamer-1.0",
      "%{IncludeDir.GStreamer}/glib-2.0",
      "%{LibraryDir.GStreamer}/glib-2.0/include",

      "%{IncludeDir.NodeEditor}",
      "%{IncludeDir.ImPlot}"
   }

   links
   {
      "Walnut"
   }

   links
   {
      "../Walnut/vendor/yaml-cpp/bin/Debug-windows-x86_64/yaml-cpp/yaml-cpp.lib",
      "%{LibraryDir.Mujoco}/mujoco.lib",
      "%{LibraryDir.GStreamer}/gstreamer-1.0.lib",
      "%{LibraryDir.GStreamer}/glib-2.0.lib",
      "%{LibraryDir.GStreamer}/gobject-2.0.lib",
      "%{LibraryDir.GStreamer}/gstapp-1.0.lib"
   }

   targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
   objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

   filter "system:windows"
      systemversion "latest"
      buildoptions { "/utf-8" }
      defines { "WL_PLATFORM_WINDOWS", "YAML_CPP_STATIC_DEFINE" }

   filter "configurations:Debug"
      defines { "WL_DEBUG" }
      runtime "Debug"
      symbols "On"
      links
      {
         "%{LibraryDir.OpenCV}/opencv_world4120d.lib",
      }

   filter "configurations:Release"
      defines { "WL_RELEASE" }
      runtime "Release"
      optimize "On"
      symbols "On"
      links
      {
         "%{LibraryDir.OpenCV}/opencv_world4120.lib",
      }

   filter "configurations:Dist"
      kind "WindowedApp"
      defines { "WL_DIST" }
      runtime "Release"
      optimize "On"
      symbols "Off"
      links
      {
         "%{LibraryDir.OpenCV}/opencv_world4120.lib",
      }