#include "VulkanRenderer.h"
#include "Vk/Utils.h"
#include "Vk/Devices/DeviceManager.h"
#include "Vk/Services/Allocator/VmaAllocatorService.h"
#include "Vk/Services/ServiceLocator.h"
#include "Log/Log.h"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_structs.hpp"

VulkanRenderer::VulkanRenderer(const std::string& title, VkCore::Window* window,
                               const std::vector<const char*>& deviceExtensions,
                               const std::vector<const char*>& instanceExtensions)
{

    for (const char* ext : deviceExtensions)
    {
        VkCore::DeviceManager::AddDeviceExtension(ext);
    }

    std::vector<const char*> reqInstanceExtensions = instanceExtensions;
    std::vector<const char*> layers;

    for (const char*& ext : VkCore::Window::GetRequiredInstanceExtensions())
    {
        reqInstanceExtensions.emplace_back(ext);
    }

#ifdef DEBUG
    reqInstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    // Initialize the Vulkan instance
    std::vector<vk::ExtensionProperties> extensionProperties = vk::enumerateInstanceExtensionProperties();

    for (const auto& extension : reqInstanceExtensions)
    {

        for (const auto& prop : extensionProperties)
        {
            if (strcmp(prop.extensionName, extension))
            {
                break;
            }

            LOGF(Vulkan, Fatal,
                 "The desired extension %s was not found! This could be due to a misspell or an outdated version!",
                 extension)
            throw std::runtime_error(
                "The desired extension was not found! This could be due to a misspell or an outdated version!");
        }
    }

    vk::ApplicationInfo appInfo = {title.c_str(), 0, title.c_str(), 0, VK_API_VERSION_1_3};

    vk::InstanceCreateInfo instanceCreateInfo({}, &appInfo);

    instanceCreateInfo.setPEnabledExtensionNames(reqInstanceExtensions);

#if DEBUG
    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo = VkCore::Utils::PopulateDebugMessengerCreateInfo();

    if (VkCore::Utils::CheckValidationLayerSupport())
    {
        instanceCreateInfo.setPEnabledLayerNames(VkCore::Utils::validationLayers);
        instanceCreateInfo.setPNext(&debugCreateInfo);
    }
#endif

    // PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNV = vkGetDeviceProcAddr(VkDevice device, const char *pName)

    TRY_CATCH_BEGIN()

    m_Instance = vk::createInstance(instanceCreateInfo);

    vk::DispatchLoaderDynamic dli{};
    dli.init();
    dli.init(m_Instance);

    TRY_CATCH_END()

#ifdef DEBUG
    // Debug messenger has to be created after creating the VkInstance!
    m_DebugMessenger = m_Instance.createDebugUtilsMessengerEXT(debugCreateInfo);
#endif

    VkSurfaceKHR surface = VK_NULL_HANDLE;

    VkResult err =
        glfwCreateWindowSurface(static_cast<VkInstance>(m_Instance), window->GetGLFWWindow(), nullptr, &surface);

    window->RefreshResolution();

    VkCore::Utils::CheckVkResult(err);
    m_Surface = surface;

    // Init Devices
    VkCore::DeviceManager::Initialize(m_Instance, m_Surface);

    // Init Services
    VkCore::VmaAllocatorService* allocationService = new VkCore::VmaAllocatorService(m_Instance);
    VkCore::ServiceLocator::ProvideAllocatorService(allocationService);

    m_Swapchain = VkCore::Swapchain(m_Surface, window->GetHeight(), window->GetWidth());
    m_RenderPass = VkCore::SwapchainRenderPass(m_Swapchain);

    CreateFramebuffers(window->GetWidth(), window->GetHeight());

    TRY_CATCH_BEGIN()

    // Sync Objects
    vk::FenceCreateInfo fenceCreateInfo{vk::FenceCreateFlagBits::eSignaled};

    vk::SemaphoreCreateInfo imageAvailableCreateInfo{};
    vk::SemaphoreCreateInfo renderFinishedCreateInfo{};

    for (int i = 0; i < m_Swapchain.GetNumberOfSwapBuffers(); i++)
    {
        m_ImageAvailableSemaphores.emplace_back(
            VkCore::DeviceManager::GetDevice().CreateSemaphore(imageAvailableCreateInfo));
        m_RenderFinishedSemaphores.emplace_back(
            VkCore::DeviceManager::GetDevice().CreateSemaphore(renderFinishedCreateInfo));

        m_InFlightFences.emplace_back(VkCore::DeviceManager::GetDevice().CreateFence(fenceCreateInfo));
    }
    TRY_CATCH_END()

    TRY_CATCH_BEGIN()

    vk::CommandPoolCreateInfo createInfo{
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        VkCore::DeviceManager::GetPhysicalDevice().GetQueueFamilyIndices().m_GraphicsFamily.value()};

    // Command Buffers
    m_CommandPool = VkCore::DeviceManager::GetDevice().CreateCommandPool(createInfo);

    vk::CommandBufferAllocateInfo allocateInfo{};

    allocateInfo.setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandPool(m_CommandPool)
        .setCommandBufferCount(m_Swapchain.GetImageViews().size());

    m_CommandBuffers = VkCore::DeviceManager::GetDevice().AllocateCommandBuffers(allocateInfo);

    TRY_CATCH_END()
}

void VulkanRenderer::CreateFramebuffers(const uint32_t width, const uint32_t height)
{
    std::vector<vk::ImageView> imageViews = m_Swapchain.GetImageViews();

    for (const vk::ImageView& view : imageViews)
    {

        vk::ImageView imageViews[] = {view, m_RenderPass.GetDepthImage().GetImageView()};

        vk::FramebufferCreateInfo createInfo{};

        createInfo.width = width;
        createInfo.height = height;
        createInfo.layers = 1;
        createInfo.renderPass = m_RenderPass.GetVkRenderPass();
        createInfo.setAttachments(imageViews);

        m_FrameBuffers.emplace_back(VkCore::DeviceManager::GetDevice().CreateFrameBuffer(createInfo));
    }
}
void VulkanRenderer::CreateSwapchain(const uint32_t width, const uint32_t height)
{
    m_Swapchain = VkCore::Swapchain(m_Surface, width, height);
    m_RenderPass = VkCore::SwapchainRenderPass(m_Swapchain);
}

void VulkanRenderer::DestroySwapchain()
{
    m_RenderPass.Destroy();
    VkCore::DeviceManager::GetDevice().DestroySwapchain(m_Swapchain.GetVkSwapchain());
}

void VulkanRenderer::DestroyFrameBuffers()
{
    VkCore::DeviceManager::GetDevice().DestroyFrameBuffers(m_FrameBuffers);
    m_FrameBuffers.clear();
}

void VulkanRenderer::BeginDraw(const vk::ClearColorValue& clearValue, const uint32_t width, const uint32_t height)
{
    vk::CommandBufferBeginInfo cmdBufferBeginInfo{};

    vk::ClearValue clearValues[2] = {};

    clearValues[0].color = clearValue;
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.f, 0);

    vk::RenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.setRenderPass(m_RenderPass.GetVkRenderPass())
        .setRenderArea(vk::Rect2D({0, 0}, {width, height}))
        .setFramebuffer(m_FrameBuffers[m_CurrentFrame])
        .setClearValues(clearValues);

    vk::CommandBuffer commandBuffer = m_CommandBuffers[m_CurrentFrame];
    commandBuffer.begin(cmdBufferBeginInfo);
    commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
}

uint32_t VulkanRenderer::EndDraw()
{
    vk::CommandBuffer commandBuffer = m_CommandBuffers[m_CurrentFrame];
    commandBuffer.endRenderPass();
    commandBuffer.end();

    vk::PipelineStageFlags dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    vk::SubmitInfo submitInfo{};
    submitInfo.setCommandBuffers(m_CommandBuffers[m_CurrentFrame])
        .setWaitSemaphores(m_ImageAvailableSemaphores[m_CurrentFrame])
        .setWaitDstStageMask(dstStageMask)
        .setSignalSemaphores(m_RenderFinishedSemaphores[m_CurrentFrame]);

    TRY_CATCH_BEGIN()

    VkCore::DeviceManager::GetDevice().GetGraphicsQueue().submit(submitInfo, m_InFlightFences[m_CurrentFrame]);

    TRY_CATCH_END()

    vk::SwapchainKHR swapchain = m_Swapchain.GetVkSwapchain();

    vk::PresentInfoKHR presentInfo{};
    presentInfo.setWaitSemaphores(m_RenderFinishedSemaphores[m_CurrentFrame])
        .setSwapchains(swapchain)
        .setImageIndices(m_CurrentFrame)
        .setPResults(nullptr);

    try
    {
        const vk::Result presentResult = VkCore::DeviceManager::GetDevice().GetPresentQueue().presentKHR(presentInfo);
    }
    catch (vk::SystemError const& err)
    {
        const int32_t resultValue = err.code().value();

        if (resultValue == (int)vk::Result::eErrorOutOfDateKHR)
        {
            return -1;
        }
        else if (resultValue != (int)vk::Result::eSuccess && resultValue != (int)vk::Result::eSuboptimalKHR)
        {
            throw std::runtime_error("Failed to acquire next swap chain image!");
        }
    }

    m_CurrentFrame = (m_CurrentFrame + 1) % m_Swapchain.GetNumberOfSwapBuffers();
    return 0;
}

void VulkanRenderer::Shutdown()
{
    VkCore::Device& device = VkCore::DeviceManager::GetDevice();
    m_RenderPass.Destroy();

    device.DestroySemaphores(m_RenderFinishedSemaphores);
    device.DestroySemaphores(m_ImageAvailableSemaphores);

    device.DestroyFences(m_InFlightFences);

    device.DestroyFrameBuffers(m_FrameBuffers);

    m_Instance.destroySurfaceKHR(m_Surface);

#ifdef DEBUG
    m_Instance.destroyDebugUtilsMessengerEXT(m_DebugMessenger);
#endif
}

uint32_t VulkanRenderer::AcquireNextImage()
{

    VkCore::DeviceManager::GetDevice().WaitForFences(m_InFlightFences[m_CurrentFrame], false);

    uint32_t imageIndex;

    try
    {
        vk::ResultValue<uint32_t> result = m_Swapchain.AcquireNextImageKHR(m_ImageAvailableSemaphores[m_CurrentFrame]);
        imageIndex = result.value;
    }
    catch (vk::SystemError const& err)
    {
        const int32_t resultValue = err.code().value();

        if (resultValue == (int)vk::Result::eErrorOutOfDateKHR)
        {
            return -1;
        }
        else if (resultValue != (int)vk::Result::eSuccess && resultValue != (int)vk::Result::eSuboptimalKHR)
        {
            throw std::runtime_error("Failed to acquire next swap chain image!");
        }
    }

    VkCore::DeviceManager::GetDevice().ResetFences(m_InFlightFences[m_CurrentFrame]);

    m_CommandBuffers[m_CurrentFrame].reset();

    return m_CurrentFrame;
}
