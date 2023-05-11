#include "core/window/windowmanager.h"
#include <GLFW/glfw3.h>

namespace app
{

WindowManager::WindowManager() :
    // _window(std::unique_ptr<GLFWwindow, WindowDeleter>{
    //         nullptr, [](GLFWwindow* window) { glfwDestroyWindow(window); }}),
    _log(logs::getLogger("WindowManager"))
{
}

WindowManager::~WindowManager()
{
    _window.reset();
    glfwTerminate();
}

WindowManager::WindowManager(WindowManager&& rhs) noexcept :
    _window(std::move(rhs._window)), _log(std::move(rhs._log))
{
}

auto WindowManager::operator=(WindowManager&& rhs) noexcept -> WindowManager&
{
    if(this != &rhs)
    {
        _window = std::move(rhs._window);
        _log = std::move(rhs._log);
    }

    return *this;
}

auto WindowManager::pollEvents() -> void
{
    glfwPollEvents();
}

[[nodiscard]] auto WindowManager::shouldClose() const -> bool
{
    return glfwWindowShouldClose(_window.get());
}

auto WindowManager::setup() -> void
{
    createWindow();
}

auto WindowManager::createWindow() -> void
{
    glfwSetErrorCallback(onErrorCallback);
    if(!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    if(!glfwVulkanSupported())
    {
        throw std::runtime_error("No Vulkan support!");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    auto* monitor = glfwGetPrimaryMonitor();
    const auto* mode = glfwGetVideoMode(monitor);
    _windowResolution.width = static_cast<uint32_t>(mode->width);
    _windowResolution.height = static_cast<uint32_t>(mode->height);

    _log->info(
            "Display resolution ({}, {})",
            _windowResolution.width,
            _windowResolution.height);

    _window.reset(glfwCreateWindow(
            static_cast<int>(_windowResolution.width),
            static_cast<int>(_windowResolution.height),
            "derps",
            nullptr,
            nullptr));

    if(!_window)
    {
        throw std::runtime_error("Failed to create GLFW window");
    }

    // Window managers may adjust window size at the time of creation, so we
    // need to ask GLFW what was the actual size
    //
    int width = 0, height = 0;
    glfwGetWindowSize(_window.get(), &width, &height);

    // Allow starting minimized?
    if(width == 0 || height == 0)
    {
        throw std::runtime_error(
                "Not allowing minimized windows at this point");
    }

    _windowResolution = {
            static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

    _log->info(
            "GLFW Window created with size of ({}, {})",
            _windowResolution.width,
            _windowResolution.height);

    glfwSetWindowUserPointer(_window.get(), this);
    glfwSetKeyCallback(_window.get(), onKeyCallback);
    glfwSetFramebufferSizeCallback(_window.get(), onFrameBufferResized);
    glfwSetCursorPosCallback(_window.get(), onCursorPositionCallback);
    glfwSetScrollCallback(_window.get(), onMouseScrollCallback);
    glfwSetMouseButtonCallback(_window.get(), onMouseButtonCallback);
}

void WindowManager::handleKeyboardInput(int key, bool isPressed)
{
    if(key == GLFW_KEY_ESCAPE && isPressed == true)
    {
        glfwSetWindowShouldClose(_window.get(), true);
    }
}

void WindowManager::handleMouseButtonInput(int button, bool isPressed)
{
    _log->info("Button({}) isPressed({})", button, isPressed);

    if(button == GLFW_MOUSE_BUTTON_LEFT)
    {
    }
    else if(button == GLFW_MOUSE_BUTTON_MIDDLE)
    {
    }
    else if(button == GLFW_MOUSE_BUTTON_RIGHT)
    {
    }
}

void WindowManager::handleMouseScrollInput(double xoffset, double yoffset)
{
    _log->info("Scroll offsets(X:{}, Y:{})", yoffset, xoffset);
}

void WindowManager::handleMousePositionInput(double xpos, double ypos)
{
    _log->info("Mouse position ({}, {})", xpos, ypos);
}

void WindowManager::onErrorCallback(int error, const char* description)
{
    spdlog::error("GLFW Error ({}): {}", error, description);
}

void WindowManager::onKeyCallback(
        GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;

    auto* windowmanager =
            reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));

    if(!windowmanager)
    {
        throw std::runtime_error("Window user pointer is null");
    }

    if(action == GLFW_PRESS)
    {
        windowmanager->handleKeyboardInput(key, true);
    }
    if(action == GLFW_RELEASE)
    {
        windowmanager->handleKeyboardInput(key, false);
    }
}

void WindowManager::onWindowResized(GLFWwindow* window, int width, int height)
{
    auto* windowmanager =
            reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));

    if(!windowmanager)
    {
        throw std::runtime_error("Window user pointer is null");
    }

    if(width == 0 || height == 0)
    {
        return;
    }

    auto log = windowmanager->getLogger();
    log->info("New window size ({}, {})", width, height);
}

void WindowManager::onFrameBufferResized(
        GLFWwindow* window, int width, int height)
{
    auto* windowmanager =
            reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));

    if(!windowmanager)
    {
        throw std::runtime_error("Window user pointer is null");
    }

    auto log = windowmanager->getLogger();
    log->info("New framebuffer size ({}, {})", width, height);

    if(width == 0 || height == 0)
    {
        return;
    }
}

void WindowManager::onCursorPositionCallback(
        GLFWwindow* window, double xpos, double ypos)
{
    auto* windowmanager =
            reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));

    if(!windowmanager)
    {
        throw std::runtime_error("Window user pointer is null");
    }

    windowmanager->handleMousePositionInput(xpos, ypos);
}

void WindowManager::onMouseButtonCallback(
        GLFWwindow* window, int button, int action, int mods)
{
    (void)mods;

    auto* windowmanager =
            reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));

    if(!windowmanager)
    {
        throw std::runtime_error("Window user pointer is null");
    }

    if(action == GLFW_PRESS)
    {
        windowmanager->handleMouseButtonInput(button, true);
    }
    else if(action == GLFW_RELEASE)
    {
        windowmanager->handleMouseButtonInput(button, false);
    }
}

void WindowManager::onMouseScrollCallback(
        GLFWwindow* window, double xoffset, double yoffset)
{
    auto* windowmanager =
            reinterpret_cast<WindowManager*>(glfwGetWindowUserPointer(window));

    if(!windowmanager)
    {
        throw std::runtime_error("Window user pointer is null");
    }

    windowmanager->handleMouseScrollInput(xoffset, yoffset);
}

} // namespace app
