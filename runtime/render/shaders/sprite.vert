#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec2 uv;
layout(location=2) in vec4 color;
layout(location=0) out vec2 texcoord;
layout(location=1) out vec4 tint;
layout(push_constant) uniform Transform { mat4 matrix; } transform;
void main() {
    gl_Position = transform.matrix * vec4(position, 1.0);
    texcoord = uv;
    tint = color;
}
