---@diagnostic disable: undefined-global
workspace "MeshAndTaskShaders"

  -- Configurations have to be defined first before including any other premake.lua files
  configurations { "Debug", "Release" }

  include "VulkanCore/premake5.lua"
  include "MeshAndTaskShaders/premake5.lua"

