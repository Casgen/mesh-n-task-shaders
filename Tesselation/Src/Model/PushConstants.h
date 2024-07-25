#pragma once

#include "glm/ext/matrix_transform.hpp"
#include "glm/mat4x4.hpp"
#include "Model/Camera.h"
#include "glm/vec3.hpp"
#include <cstdint>

struct FragmentPC {
    glm::vec3 diffusion_color;
    bool is_meshlet_view_on;
    alignas(16) glm::vec3 ambient_color;
    alignas(16) glm::vec3 specular_color;
    alignas(16) glm::vec3 direction;
    alignas(16) glm::vec3 cam_pos;
    glm::vec3 cam_view_dir;
};

struct NoisePC {
	float time;
	float speed;
	float height;
	float scale;
	int octaves;
};

struct TessPC {
	glm::uvec2 patch_count = glm::uvec2(0.f, 0.f);
};

struct InstancePC {
	uint32_t instanceCount;
};