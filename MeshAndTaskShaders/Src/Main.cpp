
#include "App/Application.h"
#include <iostream>
#include <ostream>

int main()
{
    Application app = Application(1280, 720, "Hey!");
    app.Run();
    return 0;
}
