#version 460

layout (location = 0) in vec4 i_color;
layout (location = 1) in vec3 i_normal;
layout (location = 2) in vec3 i_position;

layout (location = 0) out vec4 o_color;

// layout (binding = 0) uniform MatrixBuffer {
//     mat4 model;
//     mat4 view;
//     mat4 proj;
// } matBuffer;


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

    if (!u_meshlet_view_on) {
        o_color = vec4(normalize(i_normal) * 0.5f + 0.5f, 1.0f);
        return;
    }
    
    float diffuse = max(dot(i_normal, normalize(u_light_dir)), 0.f);

    vec3 ambient = 0.01f * u_light_color_amb;
    
    float specular = 0.f;
    float specular_light = 2.f;

    if (diffuse != 0.0) {
        vec3 view_dir = normalize(u_cam_pos - i_position);
        vec3 reflection_dir = reflect(-normalize(u_light_dir), normalize(i_normal));
        vec3 half_vec = normalize(reflection_dir) + normalize(u_light_dir);

        float spec_amount = pow(max(dot(i_normal, half_vec), 0.f), 12.f);

        specular = spec_amount * specular_light;
    }

    o_color = vec4(diffuse * u_light_color_amb + ambient + specular * u_light_color_spec, 1.0f) * i_color;
}
