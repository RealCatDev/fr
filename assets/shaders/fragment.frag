#version 450

layout(set = 1, binding = 0) uniform sampler2D uTexture;

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main() {
  outColor = texture(uTexture, inUV);
  //outColor = vec4(inUV, 0.0f, 1.0f);
}