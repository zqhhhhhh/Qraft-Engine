#include <algorithm>
#include <iostream>
#include "engine/Event.h"

std::map<std::string, std::vector<EventBus::Subscriber>> EventBus::subscriptions;
std::vector<EventBus::PendingAction> EventBus::pending_subscribes;
std::vector<EventBus::PendingAction> EventBus::pending_unsubscribes;

void EventBus::Publish(const std::string& event_type, luabridge::LuaRef event_object)
{
    auto it = subscriptions.find(event_type);
    if (it == subscriptions.end())
    {
        return;
    }
    // Copy to avoid iterator invalidation if a callback modifies subscriptions
    const std::vector<Subscriber> subscribers_copy = it->second;
    for (const auto& [component, function] : subscribers_copy)
    {
        if (!function.isFunction())
        {
            continue;
        }
        try
        {
            function(component, event_object);
        }
        catch (const luabridge::LuaException& e)
        {
            std::cout << "\033[31m" << e.what() << "\033[0m" << std::endl;
        }
    }
}

void EventBus::Subscribe(const std::string& event_type, luabridge::LuaRef component, luabridge::LuaRef function)
{
    pending_subscribes.push_back({ event_type, component, function });
}

void EventBus::Unsubscribe(const std::string& event_type, luabridge::LuaRef component, luabridge::LuaRef function)
{
    pending_unsubscribes.push_back({ event_type, component, function });
}

void EventBus::ProcessPendingSubscriptions()
{
    for (const auto& pending : pending_unsubscribes)
    {
        auto it = subscriptions.find(pending.event_type);
        if (it == subscriptions.end())
        {
            continue;
        }
        auto& subscribers = it->second;
        subscribers.erase(
            std::remove_if(subscribers.begin(), subscribers.end(),
                [&pending](const Subscriber& sub)
                {
                    return sub.first == pending.component && sub.second == pending.function;
                }),
            subscribers.end());
    }
    pending_unsubscribes.clear();

    for (const auto& pending : pending_subscribes)
    {
        subscriptions[pending.event_type].emplace_back(pending.component, pending.function);
    }
    pending_subscribes.clear();
}

void EventBus::Shutdown()
{
    subscriptions.clear();
    pending_subscribes.clear();
    pending_unsubscribes.clear();
}
