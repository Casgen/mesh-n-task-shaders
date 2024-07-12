#version 450

layout (location = 0) out vec4 fragColor;

layout (location = 0) in vec3 i_Color;

void main() {
    fragColor = vec4(i_Color, 1.0);
}
