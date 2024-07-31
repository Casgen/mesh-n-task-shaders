#version 450

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec3 a_tangent;
layout (location = 3) in vec3 a_bitangent;
layout (location = 4) in vec2 a_tex_coords;

layout (binding = 0) uniform MatrixBuffer {
    mat4 model;
    mat4 view;
    mat4 proj;
} mat_buffer;

struct s_instance_info {
	uint lod;
	uint index;
};

struct s_draw_cmd {
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	int vertexOffset;
	uint firstInstance;
};

layout (std430, set = 1, binding = 0) buffer Instances {
	mat4 instances[];
} instance_buffer;

// layout (std430, set = 1, binding = 1) buffer InstanceInfos {
// 	s_instance_info infos[];
// } instance_infos;
// 
// layout (std430, set = 1, binding = 2) buffer IndirectDrawCmds {
// 	s_draw_cmd draw_cmds[8];
// } draw_cmds;

layout (location = 0) out vec3 o_normal; 
layout (location = 1) out vec3 o_position; 

void main() {

	vec4 vertex = mat_buffer.proj * mat_buffer.view * vec4(a_position + instance_buffer.instances[gl_InstanceIndex][3].xyz, 1.f);

	gl_Position = vertex;
	o_position = vertex.xyz;
	o_normal = a_normal;
}
