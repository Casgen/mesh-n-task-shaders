#include <memory>
#include <vulkan/vulkan_core.h>

#include "Application.h"
#include "Log/Log.h"
#include "Vk/Utils.h"

Application::Application(const uint32_t width, const uint32_t height, const std::string& title)
{
    Logger::SetSeverityFilter(ESeverity::Verbose);

    VkCore::WindowProps windowProps{title, width, height};

    m_Window = std::make_unique<VkCore::Window>(m_Instance, windowProps);

    // Debug messenger has to be created after creating the VkInstance!
    m_DebugMessenger = m_Instance.createDebugUtilsMessengerEXT(VkCore::Utils::PopulateDebugMessengerCreateInfo());

    m_PhysicalDevice = VkCore::PhysicalDevice(m_Instance, m_Window->GetSurface(), m_DeviceExtensions);

    m_Device = new VkCore::Device(m_PhysicalDevice);
    m_Device->InitSwapChain(m_PhysicalDevice, m_Window->GetSurface(), width, height);
}

Application::~Application()
{
}

void Application::Run()
{
    Loop();
};

void Application::Loop()
{
    while (!m_Window->ShouldClose())
    {
    }

    m_Instance.destroy();
}
