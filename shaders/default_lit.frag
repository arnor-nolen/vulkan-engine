#version 450

// Shader input
layout (location = 0) in vec3 inColor;
// Not consumed
layout (location = 1) in vec2 texCoord;

// Output write
layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 1) uniform SceneData {
  vec4 fogColor; // w is for exponent
  vec4 fogDistances; // x for min, y for max, zw unused
  vec4 ambientColor;
  vec4 sunlightDeirection; // w for sun power
  vec4 sunlightColor;
} sceneData;

// Not consumed
layout(set = 2, binding = 0) uniform sampler2D tex1;

void main() {
  outFragColor = vec4(inColor + sceneData.ambientColor.xyz, 1.f);
}