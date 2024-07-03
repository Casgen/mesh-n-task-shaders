
#include "App/MeshApplication.h"
#include "../Vendor/ZMath/Vec3f.h"
#include <iostream>

int main(int argc, char* argv[])
{
	AABB aabb1 = {};
	AABB aabb2 = {
		.minPoint = Vec3f(-1.f) + 0.5f,
		.maxPoint = Vec3f(1.f) + 0.5f
	};

	Vec3f point = {0.5f, 0.5f, 0.5f};

	aabb1.Intersects(aabb2);

	std::cout << ":" << sizeof(Vec3f) << std::endl;

    MeshApplication app = MeshApplication();
    app.Run(1280, 720);
}
