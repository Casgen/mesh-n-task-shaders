#include "VulkanRenderer.h"
#include "Vk/Utils.h"
#include "Vk/Devices/DeviceManager.h"
#include "Vk/Services/Allocator/VmaAllocatorService.h"
#include "Vk/Services/ServiceLocator.h"
#include "Log/Log.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_structs.hpp"
#include <cstdint>

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

void VulkanRenderer::InitImGui(const VkCore::Window* window, const uint32_t width, const uint32_t height)
{
    const VkFormat requestSurfaceImageFormat[] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
                                                  VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM};
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

    // We could use the `ImGui_ImplVulkanH_CreateOrResizeWindow` function, but the problem is that it creates
    // An entirely new render pass, swapchain, framebuffers, sync objects, etc. Convenient, but we need to have
    // some kind of control over the creation of our resources. Therefore they are just passed into the
    // ImGui_ImplVulkanH_Window data. Mainly, the function doesn't create a render pass with depth.

    m_MainWindowData.Surface = m_Surface;
    m_MainWindowData.Swapchain = m_Swapchain.GetVkSwapchain();
    m_MainWindowData.SurfaceFormat = m_Swapchain.GetVkSurfaceFormat().surfaceFormat;
    m_MainWindowData.PresentMode = static_cast<VkPresentModeKHR>(m_Swapchain.GetPresentMode());
    m_MainWindowData.Width = width;
    m_MainWindowData.Height = height;
    m_MainWindowData.RenderPass = m_RenderPass.GetVkRenderPass();
    m_MainWindowData.Frames = new ImGui_ImplVulkanH_Frame[m_FrameBuffers.size()];
    m_MainWindowData.FrameIndex = m_CurrentFrame;
    m_MainWindowData.SemaphoreCount = m_FrameBuffers.size();
    m_MainWindowData.FrameSemaphores = new ImGui_ImplVulkanH_FrameSemaphores[m_FrameBuffers.size()];
    m_MainWindowData.ImageCount = m_FrameBuffers.size();
    m_MainWindowData.UseDynamicRendering = true;
    m_MainWindowData.SemaphoreIndex = m_CurrentFrame;
    m_MainWindowData.ClearEnable = false; // TODO: Check later. It might not be work properly.

    for (int i = 0; i < m_FrameBuffers.size(); i++)
    {
        const ImGui_ImplVulkanH_Frame frame = {.CommandPool = m_CommandPool,
                                               .CommandBuffer = m_CommandBuffers[i],
                                               .Fence = m_InFlightFences[i],
                                               .Backbuffer = m_Swapchain.GetImages()[i],
                                               .BackbufferView = m_Swapchain.GetImageViews()[i],
                                               .Framebuffer = m_FrameBuffers[i]};

        const ImGui_ImplVulkanH_FrameSemaphores frameSemaphores = {
            .ImageAcquiredSemaphore = m_ImageAvailableSemaphores[i],
            .RenderCompleteSemaphore = m_RenderFinishedSemaphores[i]};

        m_MainWindowData.Frames[i] = frame;
        m_MainWindowData.FrameSemaphores[i] = frameSemaphores;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;    // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;     // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;        // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;      // Enable Multi-Viewport / Platform Windows
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos; // Enable setting mouse position.

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 3.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(poolSizes);
    poolInfo.pPoolSizes = poolSizes;

    const VkResult err =
        vkCreateDescriptorPool(*VkCore::DeviceManager::GetDevice(), &poolInfo, nullptr, &m_ImGuiDescPool);
    VkCore::Utils::CheckVkResult(err);

    ImGui_ImplGlfw_InitForVulkan(window->GetGLFWWindow(), false);
    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = m_Instance;
    initInfo.PhysicalDevice = *VkCore::DeviceManager::GetPhysicalDevice();
    initInfo.Device = *VkCore::DeviceManager::GetDevice();
    initInfo.QueueFamily = VkCore::DeviceManager::GetPhysicalDevice().GetQueueFamilyIndices().m_GraphicsFamily.value();
    initInfo.Queue = VkCore::DeviceManager::GetDevice().GetGraphicsQueue();
    initInfo.PipelineCache = nullptr;
    initInfo.DescriptorPool = m_ImGuiDescPool;
    initInfo.RenderPass = m_MainWindowData.RenderPass;
    initInfo.Subpass = 0;
    initInfo.MinImageCount = m_FrameBuffers.size();
    initInfo.ImageCount = m_FrameBuffers.size();
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.Allocator = nullptr;
    initInfo.CheckVkResultFn = &VkCore::Utils::CheckVkResult;
    ImGui_ImplVulkan_Init(&initInfo);
}

void VulkanRenderer::ImGuiNewFrame(const uint32_t width, const uint32_t height)
{
    if (m_FrameBufferResized)
    {
        if (width > 0 && height > 0)
        {
            ImGui_ImplVulkan_SetMinImageCount(m_FrameBuffers.size());
            ResizeImGui(width, height);
            m_MainWindowData.FrameIndex = 0;
            m_FrameBufferResized = false;
        }
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Enable docking
    // static bool opt_fullscreen = true;
    // static bool opt_padding = false;
    // static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    //
    // ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    // if (opt_fullscreen)
    // {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    //     ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    //     ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    //     window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
    //                     ImGuiWindowFlags_NoMove;
    //     window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    // }
    // else
    // {
    //     dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
    // }
    //
    // dockspace_flags |= ImGuiDockNodeFlags_PassthruCentralNode;
    // window_flags |= ImGuiWindowFlags_NoBackground;
    //
    // bool p_open = false;
    //
    // if (!opt_padding)
    //     ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    // ImGui::Begin("DockSpace Demo", &p_open, window_flags);
    // if (!opt_padding)
    //     ImGui::PopStyleVar();
    //
    // if (opt_fullscreen)
    //     ImGui::PopStyleVar(2);
    //
    // // Submit the DockSpace
    // ImGuiIO& io = ImGui::GetIO();
    // if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    // {
    //     ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    //     ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    // }

    // ImGui::End();
}

void VulkanRenderer::ImGuiRender(const vk::CommandBuffer& cmdBuffer)
{
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();

    VkDevice device = *VkCore::DeviceManager::GetDevice();

    ImGui_ImplVulkan_RenderDrawData(drawData, cmdBuffer);

    // Update and Render additional Platform Windows
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
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

void VulkanRenderer::ResizeImGui(const uint32_t width, const uint32_t height)
{
    m_MainWindowData.Surface = m_Surface;
    m_MainWindowData.Swapchain = m_Swapchain.GetVkSwapchain();
    // m_MainWindowData.SurfaceFormat = m_Swapchain.GetVkSurfaceFormat().surfaceFormat;
    // m_MainWindowData.PresentMode = static_cast<VkPresentModeKHR>(m_Swapchain.GetPresentMode());
    m_MainWindowData.Width = width;
    m_MainWindowData.Height = height;
    m_MainWindowData.RenderPass = m_RenderPass.GetVkRenderPass();
    // m_MainWindowData.Frames = new ImGui_ImplVulkanH_Frame[m_FrameBuffers.size()];
    // m_MainWindowData.FrameIndex = m_CurrentFrame;
    // m_MainWindowData.SemaphoreCount = m_FrameBuffers.size();
    // m_MainWindowData.FrameSemaphores = new ImGui_ImplVulkanH_FrameSemaphores[m_FrameBuffers.size()];
    m_MainWindowData.ImageCount = m_FrameBuffers.size();
    // m_MainWindowData.UseDynamicRendering = true;
    // m_MainWindowData.SemaphoreIndex = m_CurrentFrame;
    // m_MainWindowData.ClearEnable = false; // TODO: Check later. It might not be work properly.

    for (int i = 0; i < m_FrameBuffers.size(); i++)
    {
        const ImGui_ImplVulkanH_Frame frame = {.CommandPool = m_CommandPool,
                                               .CommandBuffer = m_CommandBuffers[i],
                                               .Fence = m_InFlightFences[i],
                                               .Backbuffer = m_Swapchain.GetImages()[i],
                                               .BackbufferView = m_Swapchain.GetImageViews()[i],
                                               .Framebuffer = m_FrameBuffers[i]};

        m_MainWindowData.Frames[i].Backbuffer = m_Swapchain.GetImages()[i];
        m_MainWindowData.Frames[i].BackbufferView = m_Swapchain.GetImageViews()[i];
        m_MainWindowData.Frames[i].Framebuffer = m_FrameBuffers[i];
    }
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

void VulkanRenderer::BeginCmdBuffer()
{
    vk::CommandBufferBeginInfo cmdBufferBeginInfo{};

    m_CommandBuffers[m_CurrentFrame].begin(cmdBufferBeginInfo);
}
void VulkanRenderer::BeginRenderPass(const vk::ClearColorValue& clearValue, const uint32_t width, const uint32_t height)
{
    vk::ClearValue clearValues[2] = {};

    clearValues[0].color = clearValue;
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.f, 0);

    vk::RenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.setRenderPass(m_RenderPass.GetVkRenderPass())
        .setRenderArea(vk::Rect2D({0, 0}, {width, height}))
        .setFramebuffer(m_FrameBuffers[m_CurrentFrame])
        .setClearValues(clearValues);

    m_CommandBuffers[m_CurrentFrame].beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
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

void VulkanRenderer::EndRenderPass()
{
    m_CommandBuffers[m_CurrentFrame].endRenderPass();
}

int VulkanRenderer::EndCmdBuffer()
{
    vk::CommandBuffer commandBuffer = m_CommandBuffers[m_CurrentFrame];
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

    device.DestroyCommandPool(m_CommandPool);
    m_RenderPass.Destroy();

    device.DestroySemaphores(m_RenderFinishedSemaphores);
    device.DestroySemaphores(m_ImageAvailableSemaphores);

    device.DestroyFences(m_InFlightFences);

    device.DestroyFrameBuffers(m_FrameBuffers);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

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
