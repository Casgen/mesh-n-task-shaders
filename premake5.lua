---@diagnostic disable: undefined-global

workspace("MeshAndTaskShaders")

	toolset ("gcc")

	newoption {
		trigger = "with-vulkan",
		description = "Compile project with Vulkan specific mesh shader extensions and shaders, leaving null compiles with Nvidia's features"
	}

	newoption {
		trigger = "sanitize",
		description = "Compile the project with address sanitizing code for memory debugging purposes"
	}

    -- Configurations have to be defined first before including any other premake.lua files
    configurations({ "Debug", "Release" })

    include("MeshletCulling")
    include("MeshInstancing")
	include("MeshLOD")
    include("VulkanCore")

