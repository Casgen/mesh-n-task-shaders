#pragma once

#include "App/BaseApplication.h"
#include "vulkan/vulkan_core.h"
#include <cstdint>

class MeshApplication : public VkCore::BaseApplication
{
  public:
    MeshApplication(const uint32_t winWidth, const uint32_t winHeight)
        : BaseApplication(winWidth, winHeight, "Mesh and Task Shading")
    {
            m_DeviceExtensions.emplace_back(VK_EXT_mesh_shader);
    }
};
