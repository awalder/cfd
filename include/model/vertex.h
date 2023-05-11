#pragma once

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

#include "vulkan/vulkan.h"

#include <array>

namespace model
{
struct VertexPT
{
    VertexPT(glm::vec3 const& pp, glm::vec2 const& tt) : p(pp), t(tt) {}

    glm::vec3 p;
    glm::vec2 t;

    static constexpr auto getBindingDescription()
    {
        VkVertexInputBindingDescription desc;
        desc.binding = 0;
        desc.stride = sizeof(VertexPT);
        desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return desc;
    }

    static constexpr auto getAttributeDescription()
    {
        std::array<VkVertexInputAttributeDescription, 2> desc = {};
        desc[0].location = 0;
        desc[0].binding = 0;
        desc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        desc[0].offset = offsetof(VertexPT, p);

        desc[1].location = 1;
        desc[1].binding = 0;
        desc[1].format = VK_FORMAT_R32G32_SFLOAT;
        desc[1].offset = offsetof(VertexPT, t);

        return desc;
    }
};

struct VertexPC
{
    VertexPC(glm::vec3 const& pp, glm::vec3 const& cc) : p(pp), c(cc) {}

    glm::vec3 p;
    glm::vec3 c;

    static constexpr auto getBindingDescription()
    {
        VkVertexInputBindingDescription desc;
        desc.binding = 0;
        desc.stride = sizeof(VertexPC);
        desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return desc;
    }

    static constexpr auto getAttributeDescription()
    {
        std::array<VkVertexInputAttributeDescription, 2> desc = {};
        desc[0].location = 0;
        desc[0].binding = 0;
        desc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        desc[0].offset = offsetof(VertexPC, p);

        desc[1].location = 1;
        desc[1].binding = 0;
        desc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        desc[1].offset = offsetof(VertexPC, c);

        return desc;
    }
};

} // namespace model
