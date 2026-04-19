#pragma once
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "lua/lua.hpp"
#include "LuaBridge/LuaBridge.h"

class EventBus
{
public:
    static void Publish(const std::string& event_type, luabridge::LuaRef event_object);
    static void Subscribe(const std::string& event_type, luabridge::LuaRef component, luabridge::LuaRef function);
    static void Unsubscribe(const std::string& event_type, luabridge::LuaRef component, luabridge::LuaRef function);
    static void ProcessPendingSubscriptions();
    static void Shutdown();

private:
    using Subscriber = std::pair<luabridge::LuaRef, luabridge::LuaRef>;  // (component, function)

    struct PendingAction
    {
        std::string event_type;
        luabridge::LuaRef component;
        luabridge::LuaRef function;
    };

    static std::map<std::string, std::vector<Subscriber>> subscriptions;
    static std::vector<PendingAction> pending_subscribes;
    static std::vector<PendingAction> pending_unsubscribes;
};
