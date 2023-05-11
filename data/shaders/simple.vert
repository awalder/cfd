#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(binding = 0) uniform UniformBufferObject
{
    float time;
} ubo;

layout(location = 0) out vec2 outUV;

void main()
{
    outUV = inUV;
    gl_Position = vec4(inPosition, 1.0);
}

