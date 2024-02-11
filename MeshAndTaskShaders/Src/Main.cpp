
#include "App/MeshApplication.h"
//#include "App/SampleApplication.h"

int main(int argc, char* argv[])
{
    // VkCore::SampleApplication app(1280, 720, "Hello World");
    MeshApplication app(1280, 720);

    app.Run();
    return 0;
}
