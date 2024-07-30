#version 460

#extension GL_EXT_debug_printf : enable

layout (location = 0) in vec3 i_normal;
layout (location = 1) in vec3 i_position;

layout (location = 0) out vec4 o_color;

layout (push_constant, std430) uniform DirectionalLightProps {
    vec3 u_light_color_dif;
    vec3 u_light_color_amb;
    vec3 u_light_color_spec;
    vec3 u_light_dir;
    vec3 u_cam_pos;
    vec3 u_view_dir;
};


void main() {

	vec4 color = vec4(normalize(i_normal) * 0.5 + 0.5, 1.f);
    
    float diffuse = max(dot(normalize(i_normal), normalize(u_light_dir)), 0.f);

    vec3 ambient = 0.01f * u_light_color_amb;
    
    float specular = 0.f;

    if (diffuse != 0.0) {

        vec3 view_dir = normalize(u_cam_pos - i_position);
        vec3 reflection_dir = reflect(-normalize(u_light_dir), normalize(i_normal));
        vec3 half_vec = normalize(reflection_dir) + normalize(u_light_dir);

        float spec_amount = pow(max(dot(normalize(i_normal), half_vec), 0.f), 3.f);

        specular = spec_amount * 0.1f;
    }

    o_color = vec4(diffuse * u_light_color_amb + ambient + specular * u_light_color_spec, 1.0f) * color;
}
