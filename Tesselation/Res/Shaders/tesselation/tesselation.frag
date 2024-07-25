#version 460

layout (location = 0) in vec4 i_color;
layout (location = 1) in vec3 i_position;
layout (location = 2) in vec2 i_texCoords;

layout (location = 0) out vec4 o_color;

layout (set = 1, binding = 0) uniform sampler2D heightSampler;
layout (set = 1, binding = 1) uniform sampler2D normalSampler;

layout (push_constant, std430) uniform DirectionalLightProps {
    vec3 u_light_color_dif;
    bool u_meshlet_view_on;
    vec3 u_light_color_amb;
    vec3 u_light_color_spec;
    vec3 u_light_dir;
    vec3 u_cam_pos;
    vec3 u_view_dir;
};


void main() {

	vec3 tex_normal = normalize(texture(normalSampler, i_texCoords).zyx);
	vec4 tex_color = texture(heightSampler, i_texCoords);
    
    float diffuse = max(dot(tex_normal, normalize(u_light_dir)), 0.f);

    vec3 ambient = 0.01f * u_light_color_amb;
    
    float specular = 0.f;
    float specular_light = 1.f;

    if (diffuse != 0.0) {
        vec3 view_dir = normalize(u_cam_pos - i_position);
        vec3 reflection_dir = reflect(-normalize(u_light_dir), tex_normal);
        vec3 half_vec = normalize(reflection_dir) + normalize(u_light_dir);

        float spec_amount = pow(max(dot(tex_normal, half_vec), 0.f), 2.f);

        specular = spec_amount * specular_light;
    }

    o_color = vec4(diffuse + ambient + specular, 1.0f) * vec4(0.1f, 0.3f, 0.5f, 1.f);
}
