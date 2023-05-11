#pragma once

namespace event
{

struct ApplicationShutdownEvent
{
};

struct FrameBufferResizeEvent
{
    // explicit FrameBufferResizeEvent(int w, int h) : width(w), height(h) {}
    int width;
    int height;
};

} // namespace event
