#version 450

layout (location = 0) in vec3 a_Position;

layout (location = 0) out vec3 o_Color;

layout (binding = 0) uniform MatrixBuffer {
    mat4 model;
    mat4 view;
    mat4 proj;
} matBuffer;

void main() {
	
	gl_Position = matBuffer.proj * matBuffer.view * matBuffer.model * vec4(a_Position, 1.f);
	o_Color = vec3(1.f, 0.7f, 0.1f);
}
