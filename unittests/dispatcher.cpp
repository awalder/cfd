#include "gtest/gtest.h"

#include "entt/signal/dispatcher.hpp"

struct MyEvent
{
    int value;
};

void ListenerFunction(MyEvent const& event)
{
    ASSERT_EQ(event.value, 123);
}

TEST(EnTTDispatcher, TriggerEvent)
{
    entt::dispatcher dispatcher;

    dispatcher.sink<MyEvent>().connect<&ListenerFunction>();

    // This will call ListenerFunction and assert that the value is 123
    dispatcher.trigger<MyEvent>(MyEvent{.value = 123});

    dispatcher.sink<MyEvent>().disconnect<&ListenerFunction>();
}

struct ApplicationShutdownEvent
{
};

class MyApplication
{
public:
    void onApplicationShutdown(ApplicationShutdownEvent const& event) { isShutdown_ = true; }

    bool isShutdown() const { return isShutdown_; }

private:
    bool isShutdown_ = false;
};

TEST(EnTTDispatcher, ApplicationShutdownEvent)
{
    entt::dispatcher dispatcher;

    MyApplication app;

    // Connect the event listener
    dispatcher.sink<ApplicationShutdownEvent>().connect<&MyApplication::onApplicationShutdown>(
            &app);

    // Assert the application is not shut down yet
    ASSERT_FALSE(app.isShutdown());

    // Trigger the shutdown event
    dispatcher.trigger<ApplicationShutdownEvent>();

    // Assert the application is shut down
    ASSERT_TRUE(app.isShutdown());

    // Disconnect the event listener
    dispatcher.sink<ApplicationShutdownEvent>().disconnect<&MyApplication::onApplicationShutdown>(
            &app);
}
