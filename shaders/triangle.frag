#version 450

// Output write
layout (location = 0) out vec4 outFragColor;

void main() {
  // Return red color
  outFragColor = vec4(1.f, 0.f, 0.f, 1.f);
}