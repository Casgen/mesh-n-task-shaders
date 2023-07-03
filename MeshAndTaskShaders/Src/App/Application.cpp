#include <GLFW/glfw3.h>
#include <memory>
#include <vulkan/vulkan_core.h>

#include "Application.h"
#include "Log/Log.h"
#include "Vk/Utils.h"

Application::Application(const uint32_t width, const uint32_t height, const std::string &title)
{
    Log::Logger::SetSeverityFilter(Log::ESeverity::Verbose);

    VkCore::WindowProps windowProps{title, width, height};
    m_Window = std::make_unique<VkCore::Window>(m_Instance, windowProps);

    m_DebugMessenger = m_Instance.createDebugUtilsMessengerEXT(VkCore::Utils::PopulateDebugMessengerCreateInfo());
}

Application::~Application()
{
}

void Application::Run(){
    Loop();
};

void Application::Loop()
{
    while (!m_Window->ShouldClose()) {
        
    }

    m_Instance.destroy();
}
