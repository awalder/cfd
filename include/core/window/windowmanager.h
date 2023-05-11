#pragma once

#include "GLFW/glfw3.h"
#include "common/appcontext.h"
#include "core/confighandler.h"
#include "event/sub.h"
#include "logs/log.h"
#include "vulkan/vulkan.h"

#include <functional>
#include <memory>

#include "entt/signal/dispatcher.hpp"

namespace app
{

class WindowManager final
{
public:
    WindowManager(AppContextPtr const& appContext);
    ~WindowManager();

    WindowManager(WindowManager const&) = delete;
    auto operator=(WindowManager const&) -> WindowManager& = delete;

    WindowManager(WindowManager&& rhs) noexcept = delete;
    ;
    auto operator=(WindowManager&& rhs) noexcept -> WindowManager& = delete;

    auto setup() -> void;
    auto pollEvents() -> void;

    [[nodiscard]] auto shouldClose() const -> bool;
    [[nodiscard]] auto getSwapchainExtent() const -> VkExtent2D;
    [[nodiscard]] auto getWindow() const -> GLFWwindow*;

private:
    auto createWindow() -> void;
    auto getLogger() -> logs::Log& { return _log; }

    void handleKeyboardInput(int key, bool isPressed);
    void handleMouseScrollInput(double xoffset, double yoffset);
    void handleMouseButtonInput(int button, bool isPressed);
    void handleMousePositionInput(double xpos, double ypos);
    void handleFrambufferResize(int width, int height);

    static void onErrorCallback(int error, char const* description);
    static void onKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void onWindowResized(GLFWwindow* window, int width, int height);
    static void onFrameBufferResized(GLFWwindow* window, int width, int height);
    static void onCursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
    static void onMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void onMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

private:
    AppContextPtr _appContext;
    event::Subs<WindowManager> _subs;
    params::Params::ScreenConfig _config;
    using WindowDeleter = std::function<void(GLFWwindow*)>;
    std::unique_ptr<GLFWwindow, WindowDeleter> _window;
    VkExtent2D _windowResolution = {800, 800};
    logs::Log _log;
};
} // namespace app
