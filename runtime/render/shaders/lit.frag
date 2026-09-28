#version 450
layout(set=0,binding=0) uniform sampler2D picture;
layout(location=0) in vec2 texcoord;
layout(location=1) in vec4 tint;
layout(location=2) in vec3 highlight;
layout(location=3) in float fog_factor;
layout(location=4) in float fog_depth;
struct LocalLight {
    vec4 position_range;
    vec4 diffuse_attenuation0;
    vec4 ambient_attenuation1;
    vec4 attenuation2;
    vec4 direction_falloff;
    vec4 cone; // cos(theta/2),cos(phi/2),is_spot,unused
};
layout(set=1,binding=0,std140) uniform Lighting {
    vec4 ambient_count;
    LocalLight lights[8];
    vec4 viewer;
    vec4 fog_color_mode;
    vec4 fog_params;
    vec4 fog_depth_plane;
    vec4 fog_options;
} lighting;
layout(location=0) out vec4 output_color;
void main() {
    vec4 color = texture(picture,texcoord) * tint;
    vec3 rgb = clamp(color.rgb + highlight,0.0,1.0);
    int mode = int(lighting.fog_color_mode.w);
    if (mode != 0) {
        float f = fog_factor;
        if (lighting.fog_params.w != 0.0) {
            float d = abs(fog_depth);
            if (mode == 1) f = exp(-lighting.fog_params.z*d);
            else if (mode == 2) f = exp(-pow(lighting.fog_params.z*d,2.0));
            else f = (lighting.fog_params.y-d)/(lighting.fog_params.y-lighting.fog_params.x);
        }
        rgb = mix(lighting.fog_color_mode.rgb,rgb,clamp(f,0.0,1.0));
    }
    output_color = vec4(rgb,color.a);
}
