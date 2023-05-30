#extension GL_ARB_enhanced_layouts : enable
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 0) uniform UniformBufferObject
{
    vec4 color;
    ivec2 gridSize;
    float time;
    int enabled;
}
ubo;

struct GridCell
{
    vec2 velocity;
    float density;
    int solid;
    float distribution[9];
    int pad;
};
layout(std430, binding = 1) buffer Grid
{
    layout(align = 16) GridCell data[];
};

layout(push_constant) uniform PushConstants
{
    uint readBufferOffset;
    uint writeBufferOffset;
} pc;

GridCell getCell(ivec2 pos, ivec2 offset)
{
    ivec2 neighborPos = pos + offset;
    uint index = neighborPos.y * ubo.gridSize.x + neighborPos.x + pc.readBufferOffset;
    return data[index];
}

// int iron_color(float x)
// { // coloring scheme (float 0-255 -> int color)
//     x = clamp(360.0f - x * 360.0f / 255.0f, 0.0f, 360.0f);
//     float r = 255.0f, g = 0.0f, b = 0.0f;
//     if(x < 60.0f)
//     { // white - yellow
//         g = 255.0f;
//         b = 255.0f - 255.0f * x / 60.0f;
//     }
//     else if(x < 180.0f)
//     { // yellow - red
//         g = 255.0f - 255.0f * (x - 60.0f) / 120.0f;
//     }
//     else if(x < 270.0f)
//     { // red - violet
//         r = 255.0f - 255.0f * (x - 180.0f) / 180.0f;
//         b = 255.0f * (x - 180.0f) / 90.0f;
//     }
//     else
//     { // violet - black
//         r = 255.0f - 255.0f * (x - 180.0f) / 180.0f;
//         b = 255.0f - 255.0f * (x - 270.0f) / 90.0f;
//     }
//     return (((int)r) << 16) | (((int)g) << 8) | ((int)b);
// }

// density
// macro
// equilibria discrete velocities
