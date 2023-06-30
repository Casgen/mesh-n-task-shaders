#include <memory>
#include <vulkan/vulkan_core.h>

#include "Application.h"
#include "Vk/Utils.h"

Application::Application(const uint32_t width, const uint32_t height, const std::string &title)
{
    VkCore::WindowProps windowProps{title, width, height};
    m_Window = std::make_unique<VkCore::Window>(m_Instance, windowProps);
}

void Application::Run(){

};

Application::~Application()
{

}
