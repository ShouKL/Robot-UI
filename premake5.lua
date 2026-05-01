-- premake5.lua
workspace "Robot-UI"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "Robot-UI"

   -- Workspace-wide build options for MSVC
   filter "system:windows"
      buildoptions { "/EHsc", "/Zc:preprocessor", "/Zc:__cplusplus" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
include "Walnut/Build-Walnut-External.lua"

include "Robot-UI/Build-Robot-UI.lua"