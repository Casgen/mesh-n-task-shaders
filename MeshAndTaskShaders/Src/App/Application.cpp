#include <memory>

#include "Application.h"
#include "GLFW/glfw3.h"
#include "Log/Log.h"
#include "Vk/Model/SwapchainRenderPass.h"
#include "Vk/Utils.h"

Application::Application(const uint32_t width, const uint32_t height, const std::string& title)
    : m_WinWidth(width), m_WinHeight(height), m_Title(title)
{
    Logger::SetSeverityFilter(ESeverity::Verbose);
}

void Application::InitVulkan()
{

    VkCore::WindowProps windowProps{m_Title, m_WinWidth, m_WinHeight};

    m_Window = std::make_unique<VkCore::Window>(m_Instance, windowProps);

    std::vector<const char*> requiredExtensions(m_Window->GetRequiredInstanceExtensions()), layers;

#ifdef DEBUG
    requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    m_Instance = VkCore::Utils::CreateInstance(m_Title, VK_API_VERSION_1_3, requiredExtensions, layers, true);

#ifdef DEBUG
    // Debug messenger has to be created after creating the VkInstance!
    m_DebugMessenger = m_Instance.createDebugUtilsMessengerEXT(VkCore::Utils::PopulateDebugMessengerCreateInfo());
#endif

    m_Window->CreateSurface(m_Instance);

    m_PhysicalDevice = VkCore::PhysicalDevice(m_Instance, m_Window->GetSurface(), m_DeviceExtensions);
    m_Device = new VkCore::Device(m_PhysicalDevice);

    m_Device->InitSwapChain(m_PhysicalDevice, m_Window->GetSurface(), m_WinWidth, m_WinHeight);

    m_RenderPass = VkCore::SwapchainRenderPass(*m_Device);
}

Application::~Application()
{
}

void Application::Run()
{
    InitVulkan();
    Loop();
};

void Application::Loop()
{
    while (!m_Window->ShouldClose())
    {
        glfwPollEvents();
    }

    m_Instance.destroy();
}
