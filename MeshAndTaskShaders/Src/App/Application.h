#pragma once

#include <cstdint>
#include <memory>
#include <set>
#include <string>

#include "Platform/Window.h"
#include "Vk/Model/PhysicalDevice.h"
#include "vulkan/vulkan_core.h"

class Application
{
  public:
    Application(const uint32_t width, const uint32_t height, const std::string &title);
    ~Application();

    void Run();
    void Loop();

  private:
    std::set<std::string> m_DeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    vk::DebugUtilsMessengerEXT m_DebugMessenger;
    vk::Instance m_Instance;

    VkCore::PhysicalDevice m_PhysicalDevice;
    std::unique_ptr<VkCore::Window> m_Window;
};
