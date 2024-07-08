
#include "App/MeshApplication.h"
#include "../Vendor/ZMath/Vec3f.h"
#include "Model/Structures/IndexedTriangle.h"
#include <iostream>

int main(int argc, char* argv[])
{
    // AABB aabb1 = {.minPoint = Vec3f(-2.f), .maxPoint = Vec3f(2.f)};
    // AABB aabb2 = {.minPoint = Vec3f(-1.f), .maxPoint = Vec3f(1.f)};
    // AABB aabb3 = {.minPoint = Vec3f(1.f), .maxPoint = Vec3f(1.f)};
    // AABB aabb4 = {.minPoint = Vec3f(3.f), .maxPoint = Vec3f(3.f)};
    //
    // IndexedTriangle triangle1 =
    //     IndexedTriangle(Vec3f(1.61537,1.50245,1.25991), Vec3f(2.90453,5.15029,0), Vec3f(-1.29383,4.70555,0));
    //
    // IndexedTriangle triangle2 =
    //     IndexedTriangle(Vec3f(1.73129,1.13524,0), Vec3f(1.10676,-0.84971,0), Vec3f(-0.79395,1.16613,0));
    //
    // IndexedTriangle triangle3 =
    //     IndexedTriangle(Vec3f(3.0302,-5.32728,0), Vec3f(0.50448,-1.55029,2.4923), Vec3f(-1.68242,-5.61144,0));
    //
    // IndexedTriangle triangle4 =
    //     IndexedTriangle(Vec3f(-5.99398,2.2574,0), Vec3f(-2.4134,3.94138,2), Vec3f(-4.48876,4.06612,0));
    //
    //
    // const bool result1 = triangle1.Intersects(aabb1);
    // const bool result2 = triangle2.Intersects(aabb1);
    // const bool result3 = triangle3.Intersects(aabb1);
    // const bool result4 = triangle4.Intersects(aabb1);
    //
    // const bool resultaabb1 = aabb1.Intersects(aabb2);
    // const bool resultaabb2 = aabb2.Intersects(aabb3);
    // const bool resultaabb3 = aabb3.Intersects(aabb4);
    // const bool resultaabb4 = aabb4.Intersects(aabb1);
	//
    MeshApplication app = MeshApplication();
    app.Run(1280, 720);
}
