
#include <cstdint>
#include <memory>
#include <string>
#include "Platform/Window.h"

class Application
{
  public:
    Application(const uint32_t width, const uint32_t height, const std::string &title);
    ~Application();

    void Run();

  private:
    vk::Instance m_Instance;
    std::unique_ptr<VkCore::Window> m_Window;
};