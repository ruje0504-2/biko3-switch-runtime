#version 450
layout(set=0,binding=0) uniform sampler2D picture;
layout(location=0) in vec2 texcoord;
layout(location=1) in vec4 tint;
layout(location=0) out vec4 output_color;
void main() {
    output_color = texture(picture,texcoord) * tint;
}
