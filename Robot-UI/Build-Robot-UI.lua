project "Robot-UI"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"
   targetdir "bin/%{cfg.buildcfg}"
   staticruntime "off"

   files { "src/**.h", "src/**.cpp" }


   OPENCV = os.getenv("OPENCV")
   MUJOCO = os.getenv("MUJOCO")
   GSTREAMER = os.getenv("GSTREAMER")

   IncludeDir = IncludeDir or {}
   IncludeDir["OpenCV"] = "%{OPENCV}/include"
   IncludeDir["Mujoco"] = "%{MUJOCO}/include"
   IncludeDir["GStreamer"] = "%{GSTREAMER}/include"
   
   LibraryDir = LibraryDir or {}
   LibraryDir["OpenCV"] = "%{OPENCV}/x64/vc16/lib"
   LibraryDir["Mujoco"] = "%{MUJOCO}/lib"
   LibraryDir["GStreamer"] = "%{GSTREAMER}/lib"

   includedirs
   {
      "../Walnut/vendor/imgui",
      "../Walnut/vendor/glfw/include",
      "../Walnut/vendor/glm",

      "../Walnut/Walnut/Platform/GUI",
      "../Walnut/Walnut/Source",

      "../vendor/imgui-style",

      "%{IncludeDir.VulkanSDK}",
      "%{IncludeDir.OpenCV}",
      "%{IncludeDir.Mujoco}",
      "%{IncludeDir.GStreamer}/gstreamer-1.0",
      "%{IncludeDir.GStreamer}/glib-2.0",
      "%{LibraryDir.GStreamer}/glib-2.0/include",
   }

   links
   {
      "Walnut"
   }

   links
   {
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
      defines { "WL_PLATFORM_WINDOWS" }

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