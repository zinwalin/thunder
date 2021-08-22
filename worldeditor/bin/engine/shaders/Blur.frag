#pragma version

#include "Common.vert"

layout(binding = UNIFORM) uniform Uniforms {
    vec2 direction;
    vec2 size;
    int steps;
    float curve[32];
} uni;

layout(binding = UNIFORM + 1) uniform sampler2D rgbMap;

layout(location = 1) in vec2 _uv0;

layout(location = 0) out vec4 rgb;

void main (void) {
    vec4 sum = texture(rgbMap, _uv0) * uni.curve[0];
    for(int i = 1; i < uni.steps; i++) {
        vec2 offset = float(i) * uni.size * uni.direction;

        sum += texture(rgbMap, _uv0 - offset) * uni.curve[i];
        sum += texture(rgbMap, _uv0 + offset) * uni.curve[i];
    }
    rgb = sum;
}
