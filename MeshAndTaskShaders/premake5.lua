---@diagnostic disable: undefined-global
project "MeshAndTaskShaders"
  kind "ConsoleApp"

  language "C++"
  cppdialect "C++20"


  links {"VulkanCore", "GLM", "GLFW"}

  location "build"
  targetdir "bin/%{cfg.buildcfg}"
  objdir "obj/%{cfg.buildcfg}"

  includedirs {
    "../VulkanCore/Src/",
    "../VulkanCore/Vendor/glfw/include/",
    "../VulkanCore/Vendor/glm/",
    "../VulkanCore/Vendor/stb_image",
    "../VulkanCore/Vendor/vma/",
    "/home/oem/Development/VulkanSDK/1.3.250.0/x86_64/include/"
  }

  files {
    "Src/**.cpp",
    "Src/**.h",
    "Src/**.hpp"
  }

  filter "configurations:Debug"
    defines { "DEBUG" }
    symbols "on"

  filter "configurations:Release"
    defines { "NDEBUG" }
    optimize "on"

  filter { "system:linux" }
    defines {"_X11"}

    -- On linux, make sure that you have installed libvulkan-dev through your package manager!
