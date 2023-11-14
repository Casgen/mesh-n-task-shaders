---@diagnostic disable: undefined-global
project "MeshAndTaskShaders"
    kind "ConsoleApp"

    language "C++"
    cppdialect "C++17"

    architecture "x86_64"

    links {"VulkanCore", "GLM", "GLFW", "Assimp"}

    local output_dir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

    targetdir("../bin/" .. output_dir .. "/%{prj.name}")
    objdir("../obj/" .. output_dir .. "/%{prj.name}")

    includedirs {
        "../VulkanCore/Src",
        "../VulkanCore/Vendor/glfw/include",
        "../VulkanCore/Vendor/glm",
        "../VulkanCore/Vendor/stb_image",
        "../VulkanCore/Vendor/vma",
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

        -- In case of using with VulkanSDK make sure that you have VULKAN_SDK environment variable set! (for ex. /home/username/Development/VulkanSDK/1.3.250.0/x86_64)
        -- But on linux, if Vulkan is installed correctly, VulkanSDK is not needed.
        includedirs {
            "$(VULKAN_SDK)/include"
        }

        libdirs {
            "$(VULKAN_SDK)/lib"
        }

        -- On linux, make sure that you have installed libvulkan-dev through your package manager!
        links { "vulkan",  "pthread" , "shaderc_combined"}


        defines {
            "_X11",
            "_GLFW_VULKAN_STATIC"
        }

    filter { "system:windows" }

        includedirs {
            "$(VULKAN_SDK)/Include"
        }

        libdirs {
            "$(VULKAN_SDK)/Lib"
        }

        links { "vulkan-1", "shaderc_combined", "pthread" }

        defines {
            "_WIN",
            "_GLFW_VULKAN_STATIC"
        }

    -- On linux, make sure that you have installed libvulkan-dev through your package manager!
