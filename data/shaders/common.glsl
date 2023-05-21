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
    vec2 externalForce;
    ivec4 boundary;
    float divergence;
    float pressure;
    float density;
    float temperature;
};

layout(std140, binding = 1) buffer Grid
{
    GridCell data[];
};

layout(push_constant) uniform PushConstants
{
    uint readBufferOffset;
    uint writeBufferOffset;
    uint tempBufferOffset;
    uint pad;
} pc;

GridCell getCell(ivec2 pos, ivec2 offset)
{
    ivec2 neighborPos = pos + offset;
    uint index = neighborPos.y * ubo.gridSize.x + neighborPos.x + pc.readBufferOffset;
    return data[index];
}

