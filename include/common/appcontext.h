#pragma once

#include "entt/signal/dispatcher.hpp"

#include <memory>

namespace params
{
struct Params
{
    struct ScreenConfig
    {
        int width = 0;
        int height = 0;
    } screenConfig;

    struct VulkanConfig
    {
        bool vsync = false;
        bool validationLayers = false;
        bool debugUtils = false;
    } vulkanConfig;
};
} // namespace params

using ParamsPtr = std::shared_ptr<params::Params>;
using ParamsCPtr = std::shared_ptr<const params::Params>;

namespace app
{

class AppContext
{
public:
    explicit AppContext(params::Params const& params)
        : _parameters(std::make_shared<const params::Params>(params))
    {
    }

    [[nodiscard]] auto getParamsStruct() const -> ParamsCPtr;
    [[nodiscard]] auto getDispatcher() -> entt::dispatcher&;

private:
    ParamsCPtr _parameters;
    entt::dispatcher _dispatcher;
};

using AppContextPtr = std::shared_ptr<AppContext>;
using AppContextCPtr = std::shared_ptr<AppContext const>;

} // namespace app
