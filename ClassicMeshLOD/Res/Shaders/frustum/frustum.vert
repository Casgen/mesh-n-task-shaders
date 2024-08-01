#version 450

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec3 a_color;

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
	vec3 side_vec;
	float azimuth;
	float zenith;
};

layout (binding = 0) uniform MatrixBuffer {
    mat4 model;
    mat4 view;
    mat4 proj;
} mat_buffer;

layout (push_constant, std430) uniform FrustumPC {
	Frustum u_frustum;
};

layout (location = 0) out vec3 o_color;

vec3 rotateAroundAxis(const vec3 vec, const vec3 axis, const float angle) {
    vec3 normalAxis = normalize(axis);

    return vec * cos(angle) + normalAxis * dot(vec, normalAxis) * (1 - cos(angle)) +
           cross(vec, normalAxis) * sin(angle);
}

void main() {

	vec3 rotatedVert1 = rotateAroundAxis(a_position, vec3(0.f, 1.f, 0.f), u_frustum.azimuth);
	vec3 rotatedVert2 = rotateAroundAxis(rotatedVert1, u_frustum.side_vec, -u_frustum.zenith);

	gl_Position = mat_buffer.proj * mat_buffer.view * vec4(rotatedVert2 + u_frustum.point_sides, 1.f);
	o_color = a_color;
}
