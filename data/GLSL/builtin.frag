#version 150

uniform sampler2D this_frame;
in vec2 tex_coord;
out vec4 output_color;

// https://web.archive.org/web/20230325203902/https://luka712.github.io/2018/07/21/CRT-effect-Shadertoy-Unity/

/* Curvature */
const float Curvature = 6.0;

vec2 crt_coords(vec2 uv, float bend)
{
    uv -= 0.5;
    uv *= 2.;
    uv.x *= 1. + pow(abs(uv.y)/bend, 2.);
    uv.y *= 1. + pow(abs(uv.x)/bend, 2.);

    uv /= 2.;
    return uv + .5;
}

float vignette(vec2 uv, float size, float smoothness, float edgeRounding)
{
    uv -= .5;
    uv *= size;
    float amount = sqrt(pow(abs(uv.x), edgeRounding) + pow(abs(uv.y), edgeRounding));
    return smoothstep(0., smoothness, 1.0 - amount);
}

void main() {
    /* screen curvature */
    vec2 crt_uv = crt_coords(tex_coord, Curvature);
    output_color = texture(this_frame, crt_uv) * vignette(tex_coord, 1.9, 0.6, Curvature*2.0);
}
