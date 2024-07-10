
#include "App/MeshApplication.h"
#include "Mesh/MeshUtils.h"
#include "Mesh/MeshletGeneration.h"
#include "Model/Structures/Stack.h"
#include <iostream>

int main(int argc, char* argv[])
{
 //    std::vector<uint32_t> indices = {0, 1, 2, 1, 2, 4, 4, 2, 5, 4, 5, 8, 7, 4, 8, 3, 4, 7, 3, 1, 4, 6, 3, 7, 8, 5, 9};
	//
	// std::vector<uint32_t> adj = MeshletGeneration::Tipsify(indices, 10, 24);

    MeshApplication app = MeshApplication();
    app.Run(1280, 720);
}
