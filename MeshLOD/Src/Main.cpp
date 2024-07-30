
#include "App/LODApplication.h"
#include "Mesh/VertexTriangleAdjacency.h"
#include "Mesh/MeshUtils.h"

int main(int argc, char* argv[])
{
	std::vector<uint32_t> indices = {0,1,2,1,2,4,4,2,5,4,5,8,7,4,8,3,4,7,3,1,4,6,3,7,8,5,9};

	VertexTriangleAdjacency adj = MeshUtils::BuildVertexTriangleAdjacency(indices, 10);

    LODApplication app = LODApplication();
    app.Run(1280, 720);
}
