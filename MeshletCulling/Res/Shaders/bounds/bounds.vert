#version 450

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec3 a_color;

#extension GL_EXT_debug_printf : enable

struct s_meshlet_bound {
	vec3 normal;
	float cone_angle;
	vec3 sphere_pos;
	float sphere_radius;
};

layout (std430, set = 1, binding = 4) buffer MeshletBounds {
	s_meshlet_bound bounds[];	
} meshlet_bounds;

struct Frustum {
	vec3 left;
	vec3 right;
	vec3 top;
	vec3 bottom;
	vec3 front;
	vec3 back;
	vec3 point_sides;
	vec3 point_front;
	vec3 point_back;
};

layout (binding = 0) uniform MatrixBuffer {
    mat4 model;
    mat4 view;
    mat4 proj;
	Frustum frustum;
} mat_buffer;

layout (location = 0) out vec3 o_color;

void main() {
	
	s_meshlet_bound bound = meshlet_bounds.bounds[gl_InstanceIndex];

	gl_Position = mat_buffer.proj * mat_buffer.view * vec4(a_position * bound.sphere_radius + bound.sphere_pos , 1.f);
	o_color = vec3(1.f, 1.f, 1.f);
}
