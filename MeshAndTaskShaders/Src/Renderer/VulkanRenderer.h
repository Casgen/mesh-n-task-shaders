#pragma once

#include <vector>

#include "Platform/Window.h"
#include "Vk/SwapchainRenderPass.h"
#include "Vk/Swapchain.h"
#include "vulkan/vulkan_handles.hpp"

class VulkanRenderer
{

  public:
    VulkanRenderer()
    {
    }

    VulkanRenderer(const std::string& title, VkCore::Window* window, const std::vector<const char*>& deviceExtensions,
                   const std::vector<const char*>& instanceExtensions);

    void CreateFramebuffers(const uint32_t width, const uint32_t height);
    void CreateSwapchain(const uint32_t width, const uint32_t height);

    void DestroySwapchain();
    void DestroyFrameBuffers();

    void BeginDraw(const vk::ClearColorValue& clearValue, const uint32_t width, const uint32_t height);
    uint32_t EndDraw();

    void Shutdown();

    // Waits for an image to be available for rendering. **THIS HAS TO BE CALLED BEFORE DRAWING A FRAME**
    // @return - Image index. If -1 is returned, the Swapchain is out of date, and has to be recreated.
    uint32_t AcquireNextImage();

    void SetSurface(const vk::SurfaceKHR& surface)
    {
        m_Surface = surface;
    }

    // -------------- GETTERS ------------------

    VkCore::Swapchain& GetSwapchain()
    {
        return m_Swapchain;
    }

    vk::Instance& GetInstance()
    {
        return m_Instance;
    }

    vk::SurfaceKHR& GetSurface()
    {
        return m_Surface;
    }

    uint32_t GetCurrentFrame() const
    {
        return m_CurrentFrame;
    }

    VkCore::SwapchainRenderPass& GetRenderPass()
    {
        return m_RenderPass;
    }

    std::vector<vk::Framebuffer>& GetFrameBuffers()
    {
        return m_FrameBuffers;
    }

    vk::CommandBuffer GetCurrentCmdBuffer()
    {
        return m_CommandBuffers[m_CurrentFrame];
    }

    // -------------- SETTERS ------------------

    VkCore::Swapchain m_Swapchain;
    vk::Instance m_Instance = nullptr;
    vk::DebugUtilsMessengerEXT m_DebugMessenger = nullptr;
    vk::SurfaceKHR m_Surface = nullptr;

    vk::CommandPool m_CommandPool = nullptr;

    std::vector<vk::Semaphore> m_RenderFinishedSemaphores;
    std::vector<vk::Semaphore> m_ImageAvailableSemaphores;
    std::vector<vk::Fence> m_InFlightFences;
    std::vector<vk::CommandBuffer> m_CommandBuffers;

    uint32_t m_CurrentFrame = 0;
    VkCore::SwapchainRenderPass m_RenderPass;
    std::vector<vk::Framebuffer> m_FrameBuffers;
};
