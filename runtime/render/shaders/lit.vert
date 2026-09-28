#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec2 uv;
layout(location=2) in vec4 diffuse;
layout(location=3) in vec3 normal;
layout(location=4) in vec3 ambient;
layout(location=5) in vec3 emissive;
layout(location=6) in vec4 specular_power;
layout(location=0) out vec2 texcoord;
layout(location=1) out vec4 tint;
layout(location=2) out vec3 highlight;
layout(location=3) out float fog_factor;
layout(location=4) out float fog_depth;
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
layout(push_constant) uniform Transform { mat4 matrix; mat4 world; } transform;
void main() {
    gl_Position = transform.matrix * vec4(position, 1.0);
    vec4 world_position = transform.world * vec4(position, 1.0);
    // Equivalent to raw world*view XYZ before projection (no premature W
    // division). Authored accessory frames can retain W slightly below one.
    vec3 p = world_position.xyz + lighting.viewer.xyz*(1.0-world_position.w);
    mat3 basis = mat3(transform.world);
    vec3 bound = max(max(abs(basis[0]),abs(basis[1])),abs(basis[2]));
    float scale = max(max(bound.x,bound.y),bound.z);
    // Positive uniform scale cancels when normals are normalized; remove it
    // first to keep snow respawn keys away from inverse overflow/underflow.
    vec3 n = transpose(inverse(basis/scale)) * normal;
    float nlen = length(n);
    n = nlen > 0.0 ? n/nlen : vec3(0.0);
    vec3 color = emissive + ambient * lighting.ambient_count.rgb;
    vec3 specular = vec3(0.0);
    vec3 eye = lighting.viewer.xyz-p;
    float elen = length(eye);
    eye = elen > 0.0 ? eye/elen : vec3(0.0);
    for (int i=0; i<int(lighting.ambient_count.w); ++i) {
        LocalLight light = lighting.lights[i];
        vec3 delta = light.position_range.xyz-p;
        float d = length(delta);
        if (d > light.position_range.w) continue;
        float denominator = light.diffuse_attenuation0.w + light.ambient_attenuation1.w*d + light.attenuation2.x*d*d;
        if (denominator <= 0.0) continue;
        if (light.cone.z != 0.0) {
            if (d <= 0.0) continue;
            float rho = dot(-light.direction_falloff.xyz,delta/d);
            if (rho <= light.cone.y) continue;
            float spot = rho >= light.cone.x ? 1.0 :
                pow((rho-light.cone.y)/(light.cone.x-light.cone.y),light.direction_falloff.w);
            if (spot <= 0.0) continue;
            denominator /= spot;
        }
        float incidence = d > 0.0 ? max(dot(n,delta/d),0.0) : 0.0;
        color += (ambient*light.ambient_attenuation1.rgb + diffuse.rgb*light.diffuse_attenuation0.rgb*incidence)/denominator;
        if (specular_power.w >= 0.01 && incidence > 0.0 && elen > 0.0) {
            vec3 halfvector = eye + delta/d;
            float hlen = length(halfvector);
            float nh = hlen > 0.0 ? max(dot(n,halfvector/hlen),0.0) : 0.0;
            specular += specular_power.rgb * light.attenuation2.yzw * pow(nh,specular_power.w)/denominator;
        }
    }
    // Fixed-function Gouraud lighting: clamp at vertices, then interpolate.
    tint = vec4(clamp(color,0.0,1.0),diffuse.a);
    highlight = clamp(specular,0.0,1.0);
    texcoord = uv;
    fog_depth = dot(world_position, lighting.fog_depth_plane);
    fog_factor = 1.0;
    int mode = int(lighting.fog_color_mode.w);
    if (mode != 0 && lighting.fog_params.w == 0.0) {
        float d = lighting.fog_options.x != 0.0 ? elen : abs(fog_depth);
        if (mode == 1) fog_factor = exp(-lighting.fog_params.z*d);
        else if (mode == 2) fog_factor = exp(-pow(lighting.fog_params.z*d,2.0));
        else fog_factor = (lighting.fog_params.y-d)/(lighting.fog_params.y-lighting.fog_params.x);
        fog_factor = clamp(fog_factor,0.0,1.0);
    }
}
