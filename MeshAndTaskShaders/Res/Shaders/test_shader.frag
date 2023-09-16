#version 460

layout (location = 0) in vec3 i_Color;

layout (location = 0) out vec4 outColor;

void main() {
    outColor = vec4(i_Color.x, i_Color.y, i_Color.z, 1.0);
}
