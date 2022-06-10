#version 450

// Shader input
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 texCoord;

// Output write
layout (location = 0) out vec4 outFragColor;

layout(set = 0, binding = 1) uniform SceneData {
  vec4 fogColor; // w is for exponent
  vec4 fogDistances; // x for min, y for max, zw unused
  vec4 ambientColor;
  vec4 sunlightDirection; // w for sun power
  vec4 sunlightColor;
} sceneData;

layout(set = 2, binding = 0) uniform sampler2D tex;

// Same as when generating atlas
float pxRange = 4.F;

vec4 bgColor = vec4(0.F, 0.F, 0.F, 0.F);
vec4 fgColor = vec4(1.F, 1.F, 1.F, 1.F);

float screenPxRange() {
    vec2 unitRange = vec2(pxRange) / vec2(textureSize(tex, 0));
    vec2 screenTexSize = vec2(1.F) / fwidth(texCoord);
    return max(.5F * dot(unitRange, screenTexSize), 1.F);
}

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
  vec3 msd = texture(tex, texCoord).rgb;
  float sd = median(msd.r, msd.g, msd.b);
  float screenPxDistance = screenPxRange() * (sd - .5F);
  float opacity = clamp(screenPxDistance + .5F, 0.F, 1.F);
  outFragColor = mix(bgColor, fgColor, opacity);
}