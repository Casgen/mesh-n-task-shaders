---@diagnostic disable: undefined-global

workspace("MeshAndTaskShaders")

	newoption {
		trigger = 'with-vulkan',
		description = 'Compile project with Vulkan specific mesh shader extensions and shaders, leaving null compiles with Nvidia\'s features'
	}

    -- Configurations have to be defined first before including any other premake.lua files
    configurations({ "Debug", "Release" })

    include("MeshAndTaskShaders")
    include("VulkanCore")

