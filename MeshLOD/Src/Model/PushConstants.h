#pragma once

#include "glm/ext/matrix_transform.hpp"
#include "glm/mat4x4.hpp"
#include "Model/Camera.h"
#include "glm/vec3.hpp"

struct FragmentPC {
    glm::vec3 diffusion_color;
    bool is_meshlet_view_on;
    alignas(16) glm::vec3 ambient_color;
    alignas(16) glm::vec3 specular_color;
    alignas(16) glm::vec3 direction;
    alignas(16) glm::vec3 cam_pos;
    glm::vec3 cam_view_dir;
};

struct MeshPC {
    glm::mat4 rotation_mat = glm::identity<glm::mat4>();
    glm::mat4 scale_mat = glm::identity<glm::mat4>();
	uint32_t meshlet_count = 0;
};

struct InstancePC {
	uint32_t instanceCount;
};
