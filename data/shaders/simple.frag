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
    outColor = vec4(uv.x * sin(ubo.time), uv.y * sin(ubo.time), cos(ubo.time), 1.0);
    outColor = (outColor + 1 / 2);
    // outColor = texture(colorTexture, uv);
}