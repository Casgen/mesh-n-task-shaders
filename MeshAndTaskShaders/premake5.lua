---@diagnostic disable: undefined-global
project "MeshAndTaskShaders"
  kind "ConsoleApp"

  language "C++"
  cppdialect "C++20"

  location "build"
  links {"VulkanCore", "GLM", "GLFW"}

  targetdir "bin/%{cfg.buildcfg}"
  objdir "obj/%{cfg.buildcfg}"

  includedirs {
    "../VulkanCore/Src",
    "../VulkanCore/Vendor/glfw/include/",
    "../VulkanCore/Vendor/glm/",
    "../VulkanCore/Vendor/stb_image",
    "../VulkanCore/Vendor/vma/",
  }

  files {
    "%{prj.name}/Src/**.cpp",
    "%{prj.name}/Src/**.h",
    "%{prj.name}/Src/**hpp"
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
    links { "dl", "pthread", os.findlib("libvulkan-dev") }
