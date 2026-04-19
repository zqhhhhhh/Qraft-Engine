#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include "lua/lua.hpp"
#include "LuaBridge/LuaBridge.h"
#include "rapidjson/document.h"
#include "engine/Actor.h"

class ComponentDB
{
public:
    ComponentDB() = default;
    ~ComponentDB() = default;

    /* Debug API */
    static void CppLog(const std::string& message);
    static void CppLogError(const std::string& message);
    /* Application API */
    static void CppQuit();
    static void CppSleep(int milliseconds);
    static void CppOpenURL(const std::string& url);
    static int GetFrame();
    static void HideCursor();
    static void ShowCursor();

    static lua_State* GetLuaState();
    void Initialize();
    void Shutdown();
    void ClearComponentTypeCache();
    void ApplyActorJson(Actor& actor, const rapidjson::Value& actor_json) const;
    luabridge::LuaRef CreateRuntimeComponent(Actor* actor, const std::string& type_name);

    static ComponentDB* active_instance;
    static int runtime_component_counter;

private:
    static lua_State* lua_state;
    mutable std::unordered_map<std::string, std::shared_ptr<luabridge::LuaRef>> component_types;
    void InitializeState();
    void InitializeFunctions();
    void ClearCachedTypes();
    void EstablishInheritance(luabridge::LuaRef& instance_table, luabridge::LuaRef& parent_table) const;
    std::shared_ptr<luabridge::LuaRef> GetComponentType(const std::string& component_type) const;
    luabridge::LuaRef CreateComponentInstance(const std::string& component_key, const rapidjson::Value& component_json) const;
};
