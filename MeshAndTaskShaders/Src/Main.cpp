
#include "App/SampleApplication.h"
#include <iostream>
#include <ostream>
#include <unordered_set>

int main(int argc, char* argv[])
{
    VkCore::SampleApplication app = VkCore::SampleApplication(1280, 720, "Sample");
    //VkCore::Application app = VkCore::Application(1280, 720, "Hey!");
    app.Run();
    return 0;
}
