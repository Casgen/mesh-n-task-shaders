
#include "App/BaseApplication.h"
#include <iostream>
#include <ostream>

int main()
{
    VkCore::BaseApplication app = VkCore::BaseApplication(1280, 720, "Hey!");
    app.Run();
    return 0;
}
