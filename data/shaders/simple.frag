#version 450

layout(location = 0) in vec2 uv;

layout(binding = 0) uniform UniformBufferObject
{
    float time;
} ubo;

layout(binding = 1) uniform sampler2D colorTexture;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(colorTexture, uv);
}
