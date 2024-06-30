
#include "App/MeshApplication.h"
#include "../Vendor/ZMath/Vec3f.h"
#include <iostream>

int main(int argc, char* argv[])
{
	Vec3f a = {1.f, 2.f, 3.f};
	Vec3f b = {3.f, 3.f, 4.f};

	float dot = a.Dot(b);
	Vec3f cross = a.Cross(b);

	std::cout << dot << std::endl;

	cross.Print();

	std::cout << ":" << sizeof(Vec3f) << std::endl;


    MeshApplication app = MeshApplication();
    app.Run(1280, 720);
}
