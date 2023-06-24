
#include "App/Application.h"
#include <iostream>
#include <ostream>

int main()
{
    Application app = Application(1280, 720, "Hey!");

    vk::ApplicationInfo appInfo = {"Some name", 0, "Engine name", 0, VK_API_VERSION_1_3};

    vk::InstanceCreateInfo InstanceCreateInfo({}, &appInfo);

    vk::createInstance(InstanceCreateInfo);

    return 0;
    
}
