#version 460

layout (location = 0) in vec4 fragIn; 

layout (location = 0) out vec4 FragColor;

void main()
{
  FragColor = fragIn;
}
