#version 450

layout(binding = 0) uniform CameraUBO {
  mat4 proj;
  mat4 view;
  mat4 model;
} ubo;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;

layout (location = 0) out vec2 outUV;

void main() {
  gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPos, 1.0);
  outUV = inUV;
}