#pragma once

#include "entt/core/utility.hpp"
#include "entt/signal/delegate.hpp"
#include "entt/signal/dispatcher.hpp"

namespace event
{

template<typename Listener>
class Subs
{
public:
    Subs(Subs const&) = delete;
    auto operator=(Subs const&) -> Subs& = delete;
    Subs(Subs&&) noexcept = delete;
    auto operator=(Subs&&) noexcept -> Subs& = delete;

    ~Subs() { detachAll(); }
    explicit Subs(entt::dispatcher& d, Listener* const listener)
        : _dispatcher(d), _listener(listener)
    {
    }

    template<typename Event>
    void attach()
    {
        _dispatcher.sink<Event>()
                .template connect<entt::overload<void(Event const&)>(&Listener::onEvent)>(
                        _listener);
    }

    template<typename Event>
    void detach()
    {
        _dispatcher.sink<Event>()
                .template disconnect<entt::overload<void(Event const&)>(&Listener::onEvent)>(
                        _listener);
    }

    void detachAll() { _dispatcher.disconnect(_listener); }

    template<typename Event>
    void enqueue(Event&& event)
    {
        _dispatcher.enqueue<Event>(std::forward(event));
    }

    template<typename Event>
    void trigger(Event&& event)
    {
        _dispatcher.trigger<Event>(std::forward(event));
    }

private:
    entt::dispatcher& _dispatcher;
    Listener* const _listener;
};

} // namespace event
