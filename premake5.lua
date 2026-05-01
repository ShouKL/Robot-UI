-- premake5.lua
workspace "Robot-UI"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "Robot-UI"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
include "vendor/Walnut/Build-Walnut-External.lua"

include "Robot-UI"