#pragma once

#include <cstdint>
#include <memory>
#include <set>
#include <string>

#include "Vk/Model/Device.h"
#include "Platform/Window.h"
#include "Vk/Model/RenderPass.h"
#include "vulkan/vulkan_core.h"

class Application
{
  public:
    Application(const uint32_t width, const uint32_t height, const std::string& title);
    ~Application();

    void InitVulkan();
    void Run();
    void Loop();

  private:
    std::set<std::string> m_DeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    vk::DebugUtilsMessengerEXT m_DebugMessenger;
    vk::Instance m_Instance;
    VkCore::RenderPass m_RenderPass;

    VkCore::PhysicalDevice m_PhysicalDevice;
    VkCore::Device* m_Device;

    std::unique_ptr<VkCore::Window> m_Window;
    uint32_t m_WinWidth, m_WinHeight;
    std::string m_Title;
};
