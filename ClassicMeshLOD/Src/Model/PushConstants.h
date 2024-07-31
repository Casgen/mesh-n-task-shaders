#pragma once

#include "glm/ext/matrix_transform.hpp"
#include "glm/mat4x4.hpp"
#include "Model/Camera.h"
#include "glm/vec3.hpp"

struct FragmentPC {
    glm::vec3 diffusion_color;
    alignas(16) glm::vec3 ambient_color;
    alignas(16) glm::vec3 specular_color;
    alignas(16) glm::vec3 direction;
    alignas(16) glm::vec3 cam_pos;
    alignas(16) glm::vec3 cam_view_dir;
};

struct LodPC {
	Frustum frustum = {};
	uint32_t lod_count = 0;
	uint32_t max_instances_count = 0;
	uint32_t instances_count = 0;
	float lod_pow = 0.7f;
	uint32_t enable_culling = false;
};
