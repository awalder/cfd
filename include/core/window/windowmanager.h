#pragma once

#include "logs/log.h"
#include "GLFW/glfw3.h"

#include <functional>
#include <memory>
#include <vulkan/vulkan.h>

namespace app
{

class WindowManager final
{
public:
    WindowManager();
    ~WindowManager();

    WindowManager(const WindowManager&) = delete;
    auto operator=(const WindowManager&) -> WindowManager& = delete;

    WindowManager(WindowManager&& rhs) noexcept;
    auto operator=(WindowManager&& rhs) noexcept -> WindowManager&;

    auto setup() -> void;
    auto pollEvents() -> void;
    [[nodiscard]] auto shouldClose() const -> bool;

private:
    auto createWindow() -> void;
    auto getLogger() -> logs::Log& { return _log; }

    void handleKeyboardInput(int key, bool isPressed);
    void handleMouseScrollInput(double xoffset, double yoffset);
    void handleMouseButtonInput(int button, bool isPressed);
    void handleMousePositionInput(double xpos, double ypos);

    static void onErrorCallback(int error, const char* description);
    static void onKeyCallback(
            GLFWwindow* window, int key, int scancode, int action, int mods);
    static void onWindowResized(GLFWwindow* window, int width, int height);
    static void onFrameBufferResized(GLFWwindow* window, int width, int height);
    static void onCursorPositionCallback(
            GLFWwindow* window, double xpos, double ypos);
    static void onMouseButtonCallback(
            GLFWwindow* window, int button, int action, int mods);
    static void onMouseScrollCallback(
            GLFWwindow* window, double xoffset, double yoffset);

private:
    using WindowDeleter = std::function<void(GLFWwindow*)>;
    std::unique_ptr<GLFWwindow, WindowDeleter> _window;
    VkExtent2D _windowResolution = {800, 800};
    logs::Log _log;
};
} // namespace app
