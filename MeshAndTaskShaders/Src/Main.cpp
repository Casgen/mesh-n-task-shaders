
#include "App/BaseApplication.h"
#include <iostream>
#include <ostream>
#include <unordered_set>

int main()
{
    VkCore::Application app = VkCore::Application(1280, 720, "Hey!");
    app.Run();
    return 0;
}
