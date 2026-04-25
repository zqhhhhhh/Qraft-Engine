#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <thread>
#include <chrono>
#include "engine/ComponentDB.h"
#include "engine/SceneDB.h"
#include "inputManager/Input.h"
#include "autograder/Helper.h"
#include "physics/Rigidbody.h"
#include "physics/Raycast.h"
#include "engine/Event.h"
#include "renderer/Renderer.h"
#include "renderer/TextDB.h"
#include "renderer/AudioDB.h"
#include "renderer/ImageDB.h"
#include "particle/ParticleSystem.h"

lua_State* ComponentDB::lua_state = nullptr;
ComponentDB* ComponentDB::active_instance = nullptr;
int ComponentDB::runtime_component_counter = 0;

namespace
{
    void populate_lua_array(lua_State* lua_state, luabridge::LuaRef& table, const rapidjson::Value& value);

    void apply_rigidbody_json(Rigidbody* rb, const rapidjson::Value& component_json)
    {
        for (auto it = component_json.MemberBegin(); it != component_json.MemberEnd(); ++it)
        {
            const std::string prop = it->name.GetString();
            if (prop == "type") continue;
            const rapidjson::Value& val = it->value;
            if      (prop == "x"               && val.IsNumber()) rb->x               = static_cast<float>(val.GetDouble());
            else if (prop == "y"               && val.IsNumber()) rb->y               = static_cast<float>(val.GetDouble());
            else if (prop == "body_type"        && val.IsString()) rb->body_type        = val.GetString();
            else if (prop == "precise"          && val.IsBool())   rb->precise          = val.GetBool();
            else if (prop == "gravity_scale"    && val.IsNumber()) rb->gravity_scale    = static_cast<float>(val.GetDouble());
            else if (prop == "density"          && val.IsNumber()) rb->density          = static_cast<float>(val.GetDouble());
            else if (prop == "angular_friction" && val.IsNumber()) rb->angular_friction = static_cast<float>(val.GetDouble());
            else if (prop == "rotation"         && val.IsNumber()) rb->rotation         = static_cast<float>(val.GetDouble());
            else if (prop == "collider_type"    && val.IsString()) rb->collider_type    = val.GetString();
            else if (prop == "width"            && val.IsNumber()) rb->width            = static_cast<float>(val.GetDouble());
            else if (prop == "height"           && val.IsNumber()) rb->height           = static_cast<float>(val.GetDouble());
            else if (prop == "radius"           && val.IsNumber()) rb->radius           = static_cast<float>(val.GetDouble());
            else if (prop == "trigger_type"     && val.IsString()) rb->trigger_type     = val.GetString();
            else if (prop == "trigger_width"    && val.IsNumber()) rb->trigger_width    = static_cast<float>(val.GetDouble());
            else if (prop == "trigger_height"   && val.IsNumber()) rb->trigger_height   = static_cast<float>(val.GetDouble());
            else if (prop == "trigger_radius"   && val.IsNumber()) rb->trigger_radius   = static_cast<float>(val.GetDouble());
            else if (prop == "friction"         && val.IsNumber()) rb->friction         = static_cast<float>(val.GetDouble());
            else if (prop == "bounciness"       && val.IsNumber()) rb->bounciness       = static_cast<float>(val.GetDouble());
            else if (prop == "has_collider"     && val.IsBool())   rb->has_collider     = val.GetBool();
            else if (prop == "has_trigger"      && val.IsBool())   rb->has_trigger      = val.GetBool();
        }
    }

    luabridge::LuaRef rigidbody_to_lua_ref(lua_State* L, Rigidbody* rb)
    {
        return luabridge::LuaRef(L, rb);
    }

    void apply_particle_system_json(ParticleSystem* ps, const rapidjson::Value& component_json)
    {
        for (auto it = component_json.MemberBegin(); it != component_json.MemberEnd(); ++it)
        {
            const std::string prop = it->name.GetString();
            if (prop == "type") continue;
            const rapidjson::Value& val = it->value;
            if      (prop == "x"                   && val.IsNumber()) ps->x                   = static_cast<float>(val.GetDouble());
            else if (prop == "y"                   && val.IsNumber()) ps->y                   = static_cast<float>(val.GetDouble());
            else if (prop == "emit_angle_min"       && val.IsNumber()) ps->emit_angle_min       = static_cast<float>(val.GetDouble());
            else if (prop == "emit_angle_max"       && val.IsNumber()) ps->emit_angle_max       = static_cast<float>(val.GetDouble());
            else if (prop == "emit_radius_min"      && val.IsNumber()) ps->emit_radius_min      = static_cast<float>(val.GetDouble());
            else if (prop == "emit_radius_max"      && val.IsNumber()) ps->emit_radius_max      = static_cast<float>(val.GetDouble());
            else if (prop == "frames_between_bursts" && val.IsInt())   ps->frames_between_bursts = std::max(1, val.GetInt());
            else if (prop == "burst_quantity"       && val.IsInt())    ps->burst_quantity       = std::max(1, val.GetInt());
            else if (prop == "start_scale_min"      && val.IsNumber()) ps->start_scale_min      = static_cast<float>(val.GetDouble());
            else if (prop == "start_scale_max"      && val.IsNumber()) ps->start_scale_max      = static_cast<float>(val.GetDouble());
            else if (prop == "rotation_min"         && val.IsNumber()) ps->rotation_min         = static_cast<float>(val.GetDouble());
            else if (prop == "rotation_max"         && val.IsNumber()) ps->rotation_max         = static_cast<float>(val.GetDouble());
            else if (prop == "start_speed_min"      && val.IsNumber()) ps->start_speed_min      = static_cast<float>(val.GetDouble());
            else if (prop == "start_speed_max"      && val.IsNumber()) ps->start_speed_max      = static_cast<float>(val.GetDouble());
            else if (prop == "rotation_speed_min"   && val.IsNumber()) ps->rotation_speed_min   = static_cast<float>(val.GetDouble());
            else if (prop == "rotation_speed_max"   && val.IsNumber()) ps->rotation_speed_max   = static_cast<float>(val.GetDouble());
            else if (prop == "start_color_r"        && val.IsInt())    ps->start_color_r        = std::clamp(val.GetInt(), 0, 255);
            else if (prop == "start_color_g"        && val.IsInt())    ps->start_color_g        = std::clamp(val.GetInt(), 0, 255);
            else if (prop == "start_color_b"        && val.IsInt())    ps->start_color_b        = std::clamp(val.GetInt(), 0, 255);
            else if (prop == "start_color_a"        && val.IsInt())    ps->start_color_a        = std::clamp(val.GetInt(), 0, 255);
            else if (prop == "duration_frames"      && val.IsInt())    ps->duration_frames      = std::max(1, val.GetInt());
            else if (prop == "gravity_scale_x"      && val.IsNumber()) ps->gravity_scale_x      = static_cast<float>(val.GetDouble());
            else if (prop == "gravity_scale_y"      && val.IsNumber()) ps->gravity_scale_y      = static_cast<float>(val.GetDouble());
            else if (prop == "drag_factor"          && val.IsNumber()) ps->drag_factor          = static_cast<float>(val.GetDouble());
            else if (prop == "angular_drag_factor"  && val.IsNumber()) ps->angular_drag_factor  = static_cast<float>(val.GetDouble());
            else if (prop == "end_scale"            && val.IsNumber()) ps->end_scale            = static_cast<float>(val.GetDouble());
            else if (prop == "end_color_r"          && val.IsInt())    ps->end_color_r          = val.GetInt();
            else if (prop == "end_color_g"          && val.IsInt())    ps->end_color_g          = val.GetInt();
            else if (prop == "end_color_b"          && val.IsInt())    ps->end_color_b          = val.GetInt();
            else if (prop == "end_color_a"          && val.IsInt())    ps->end_color_a          = val.GetInt();
            else if (prop == "image"                && val.IsString()) ps->image                = val.GetString();
            else if (prop == "sorting_order"        && val.IsInt())    ps->sorting_order        = val.GetInt();
        }
    }

    luabridge::LuaRef particle_system_to_lua_ref(lua_State* L, ParticleSystem* ps)
    {
        return luabridge::LuaRef(L, ps);
    }

    void populate_lua_value(lua_State* lua_state, luabridge::LuaRef& table, const std::string& key, const rapidjson::Value& value)
    {
        if (value.IsNull())
        {
            table[key] = luabridge::Nil();
        }
        else if (value.IsBool())
        {
            table[key] = value.GetBool();
        }
        else if (value.IsInt())
        {
            table[key] = value.GetInt();
        }
        else if (value.IsUint())
        {
            table[key] = value.GetUint();
        }
        else if (value.IsInt64())
        {
            table[key] = value.GetInt64();
        }
        else if (value.IsUint64())
        {
            table[key] = value.GetUint64();
        }
        else if (value.IsNumber())
        {
            table[key] = value.GetDouble();
        }
        else if (value.IsString())
        {
            table[key] = std::string(value.GetString());
        }
        else if (value.IsObject())
        {
            luabridge::LuaRef nested = luabridge::newTable(lua_state);
            for (auto it = value.MemberBegin(); it != value.MemberEnd(); ++it)
            {
                populate_lua_value(lua_state, nested, it->name.GetString(), it->value);
            }
            table[key] = nested;
        }
        else if (value.IsArray())
        {
            luabridge::LuaRef nested = luabridge::newTable(lua_state);
            populate_lua_array(lua_state, nested, value);
            table[key] = nested;
        }
    }

    void populate_lua_array(lua_State* lua_state, luabridge::LuaRef& table, const rapidjson::Value& value)
    {
        int index = 1;
        for (const auto& element : value.GetArray())
        {
            if (element.IsNull())
            {
                table[index] = luabridge::Nil();
            }
            else if (element.IsBool())
            {
                table[index] = element.GetBool();
            }
            else if (element.IsInt())
            {
                table[index] = element.GetInt();
            }
            else if (element.IsUint())
            {
                table[index] = element.GetUint();
            }
            else if (element.IsInt64())
            {
                table[index] = element.GetInt64();
            }
            else if (element.IsUint64())
            {
                table[index] = element.GetUint64();
            }
            else if (element.IsNumber())
            {
                table[index] = element.GetDouble();
            }
            else if (element.IsString())
            {
                table[index] = std::string(element.GetString());
            }
            else if (element.IsObject())
            {
                luabridge::LuaRef nested = luabridge::newTable(lua_state);
                for (auto it = element.MemberBegin(); it != element.MemberEnd(); ++it)
                {
                    populate_lua_value(lua_state, nested, it->name.GetString(), it->value);
                }
                table[index] = nested;
            }
            else if (element.IsArray())
            {
                luabridge::LuaRef nested = luabridge::newTable(lua_state);
                populate_lua_array(lua_state, nested, element);
                table[index] = nested;
            }
            ++index;
        }
    }
}

lua_State* ComponentDB::GetLuaState()
{
    return lua_state;
}

void ComponentDB::Initialize()
{
    active_instance = this;
    InitializeState();
    InputManager::Init();
    InitializeFunctions();
}

void ComponentDB::InitializeState()
{
    if (lua_state != nullptr)
    {
        return;
    }
    lua_state = luaL_newstate();
    if (lua_state == nullptr)
    {
        exit(0);
    }
    luaL_openlibs(lua_state);
}

void ComponentDB::InitializeFunctions()
{
    if (lua_state == nullptr)
    {
        return;
    }
    /* Expose C++ classes to lua */
    luabridge::getGlobalNamespace(lua_state)
        .beginClass<glm::vec2>("vec2")
        .addProperty("x", &glm::vec2::x)
        .addProperty("y", &glm::vec2::y)
        .endClass();
    luabridge::getGlobalNamespace(lua_state)
        .beginClass<b2Vec2>("Vector2")
        .addConstructor<void (*)(float, float)>()
        .addProperty("x", &b2Vec2::x)
        .addProperty("y", &b2Vec2::y)
        .addFunction("Normalize", &b2Vec2::Normalize)
        .addFunction("Length", &b2Vec2::Length)
        .addFunction("__add", &b2Vec2::Add)
        .addFunction("__sub", &b2Vec2::Subtract)
        .addFunction("__mul", &b2Vec2::Multiply)
        .addStaticFunction("Distance", static_cast<float (*)(const b2Vec2&, const b2Vec2&)>(&b2Distance))
        .addStaticFunction("Dot", static_cast<float (*)(const b2Vec2&, const b2Vec2&)>(&b2Dot))
        .endClass();
    luabridge::getGlobalNamespace(lua_state)
        .beginClass<Actor>("Actor")
        .addFunction("GetName", &Actor::GetName)
        .addFunction("GetID", &Actor::GetID)
        .addFunction("GetComponentByKey", &Actor::GetComponentByKey)
        .addFunction("GetComponent", &Actor::GetComponent)
        .addFunction("GetComponents", &Actor::GetComponents)
        .addFunction("AddComponent", &Actor::AddComponent)
        .addFunction("RemoveComponent", &Actor::RemoveComponent)
        .endClass();
    Rigidbody::RegisterWithLua(lua_state);
    ParticleSystem::RegisterWithLua(lua_state);
    /* Register HitResult struct */
    luabridge::getGlobalNamespace(lua_state)
        .beginClass<HitResult>("HitResult")
        .addProperty("actor", &HitResult::actor)
        .addProperty("point", &HitResult::point)
        .addProperty("normal", &HitResult::normal)
        .addProperty("is_trigger", &HitResult::is_trigger)
        .endClass();
    /* Expose Physics namespace */
    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace("Physics")
        .addFunction("Raycast", &Raycast::Raycast_Single)
        .addFunction("RaycastAll", &Raycast::Raycast_All)
        .endNamespace();
    /* Expose C++ functions to lua */
    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace("Actor")
        .addFunction("Find", SceneDB::FindActorByName)
        .addFunction("FindAll", SceneDB::FindActorsByName)
        .addFunction("Instantiate", SceneDB::InstantiateActor)
        .addFunction("Destroy", SceneDB::DestroyActor)
        .endNamespace();
    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace("Debug")
        .addFunction("Log", ComponentDB::CppLog)
        .addFunction("LogError", ComponentDB::CppLogError)
        .endNamespace();
    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace("Application")
        .addFunction("Quit", ComponentDB::CppQuit)
        .addFunction("Sleep", ComponentDB::CppSleep)
        .addFunction("OpenURL", ComponentDB::CppOpenURL)
        .addFunction("GetFrame", ComponentDB::GetFrame)
        .endNamespace();
    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace("Input")
        .addFunction("GetKey", InputManager::GetKey)
        .addFunction("GetKeyDown", InputManager::GetKeyDown)
        .addFunction("GetKeyUp", InputManager::GetKeyUp)
        .addFunction("GetMousePosition", InputManager::GetMousePosition)
        .addFunction("GetMouseButton", InputManager::GetMouseButton)
        .addFunction("GetMouseButtonDown", InputManager::GetMouseButtonDown)
        .addFunction("GetMouseButtonUp", InputManager::GetMouseButtonUp)
        .addFunction("GetMouseScrollDelta", InputManager::GetMouseScrollDelta)
        .addFunction("HideCursor", &ComponentDB::HideCursor)
        .addFunction("ShowCursor", &ComponentDB::ShowCursor)
        .endNamespace();
    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace("Text")
        .addFunction("Draw", TextDB::LuaDrawText)
        .endNamespace();
    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace("Audio")
        .addFunction("Play", AudioDB::LuaPlayAudio)
        .addFunction("Halt", AudioDB::LuaHaltAudio)
        .addFunction("SetVolume", AudioDB::LuaSetVolume)
        .endNamespace();
    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace("Image")
        .addFunction("DrawUI", ImageDB::LuaDrawUI)
        .addFunction("DrawUIEx", ImageDB::LuaDrawUIEx)
        .addFunction("Draw", ImageDB::LuaDraw)
        .addFunction("DrawEx", ImageDB::LuaDrawEx)
        .addFunction("DrawPixel", ImageDB::LuaDrawPixel)
        .endNamespace();
    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace("Camera")
        .addFunction("SetPosition", Renderer::LuaSetCameraPosition)
        .addFunction("GetPositionX", Renderer::LuaGetCameraPositionX)
        .addFunction("GetPositionY", Renderer::LuaGetCameraPositionY)
        .addFunction("SetZoom", Renderer::LuaSetCameraZoom)
        .addFunction("GetZoom", Renderer::LuaGetCameraZoom)
        .endNamespace();
    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace("Scene")
        .addFunction("Load", SceneDB::LuaLoadScene)
        .addFunction("GetCurrent", SceneDB::LuaGetCurrentScene)
        .addFunction("DontDestroy", SceneDB::LuaDontDestroyOnLoad)
        .endNamespace();
    luabridge::getGlobalNamespace(lua_state)
        .beginNamespace("Event")
        .addFunction("Publish", &EventBus::Publish)
        .addFunction("Subscribe", &EventBus::Subscribe)
        .addFunction("Unsubscribe", &EventBus::Unsubscribe)
        .endNamespace();
}

void ComponentDB::Shutdown()
{
    Rigidbody::Shutdown();
    ParticleSystem::Shutdown();
    EventBus::Shutdown();
    ClearCachedTypes();
    if (lua_state != nullptr)
    {
        lua_close(lua_state);
        lua_state = nullptr;
    }
}

void ComponentDB::ClearComponentTypeCache()
{
    ClearCachedTypes();
}

void ComponentDB::ClearCachedTypes() 
{ 
    component_types.clear(); 
}

/* Debug API */
void ComponentDB::CppLog(const std::string& message) { std::cout << message << std::endl; }
void ComponentDB::CppLogError(const std::string& message) { std::cout << message << std::endl; }

/* Application API */
void ComponentDB::CppQuit() { exit(0); }
void ComponentDB::CppSleep(int milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }
int ComponentDB::GetFrame() { return Helper::GetFrameNumber(); }
void ComponentDB::CppOpenURL(const std::string& url)
{
#if defined(_WIN32)
    std::string command = "start " + url;
    system(command.c_str());
#elif defined(__APPLE__)
    std::string command = "open " + url;
    system(command.c_str());
#elif defined(__linux__)
    std::string command = "xdg-open " + url;
    system(command.c_str());
#endif
}

/* Input API (More in InputManager/Input.h) */
void ComponentDB::HideCursor() { SDL_ShowCursor(SDL_DISABLE); }
void ComponentDB::ShowCursor() { SDL_ShowCursor(SDL_ENABLE); }

void ComponentDB::EstablishInheritance(luabridge::LuaRef& instance_table, luabridge::LuaRef& parent_table) const
{
    luabridge::LuaRef new_metatable = luabridge::newTable(lua_state);
    new_metatable["__index"] = parent_table;
    instance_table.push(lua_state);
    new_metatable.push(lua_state);
    lua_setmetatable(lua_state, -2);
    lua_pop(lua_state, 1);
}

std::shared_ptr<luabridge::LuaRef> ComponentDB::GetComponentType(const std::string& component_type) const
{
    auto found = component_types.find(component_type);
    if (found != component_types.end())
    {
        return found->second;
    
}    const std::filesystem::path component_path = std::filesystem::path("resources/component_types") / (component_type + ".lua");
    if (!std::filesystem::exists(component_path))
    {
        throw std::runtime_error("error: failed to locate component " + component_type);
    }
    if (luaL_dofile(lua_state, component_path.string().c_str()) != LUA_OK)
    {
        const char* lua_err = lua_tostring(lua_state, -1);
        std::string msg = lua_err ? std::string(lua_err)
                                  : ("problem with lua file " + component_path.stem().string());
        lua_pop(lua_state, 1);
        throw std::runtime_error(msg);
    }
    luabridge::LuaRef component_table = luabridge::getGlobal(lua_state, component_type.c_str());
    if (!component_table.isTable())
    {
        throw std::runtime_error(
            "problem with lua file " + component_path.stem().string() +
            ": global '" + component_type + "' is not a table — "
            "make sure the variable name in the file matches the filename");
    }
    auto stored = std::make_shared<luabridge::LuaRef>(std::move(component_table));
    component_types.emplace(component_type, stored);
    return stored;
}

luabridge::LuaRef ComponentDB::CreateComponentInstance(const std::string& component_key, const rapidjson::Value& component_json) const
{
    const std::string component_type = component_json["type"].GetString();

    // ---- C++ built-in component: Rigidbody ----
    if (component_type == "Rigidbody")
    {
        Rigidbody* rb = Rigidbody::NewInstance();
        rb->key  = component_key;
        apply_rigidbody_json(rb, component_json);
        return rigidbody_to_lua_ref(lua_state, rb);
    }

    // ---- C++ built-in component: ParticleSystem ----
    if (component_type == "ParticleSystem")
    {
        ParticleSystem* ps = ParticleSystem::NewInstance();
        ps->key = component_key;
        apply_particle_system_json(ps, component_json);
        return particle_system_to_lua_ref(lua_state, ps);
    }

    std::shared_ptr<luabridge::LuaRef> base_table = GetComponentType(component_type);
    luabridge::LuaRef instance = luabridge::newTable(lua_state);
    instance["key"] = component_key;
    instance["type"] = component_type;
    instance["enabled"] = true;
    for (auto it = component_json.MemberBegin(); it != component_json.MemberEnd(); ++it)
    {
        if (std::string(it->name.GetString()) == "type")
        {
            continue;
        }
        populate_lua_value(lua_state, instance, it->name.GetString(), it->value);
    }
    EstablishInheritance(instance, *base_table);
    return instance;
}

luabridge::LuaRef ComponentDB::CreateRuntimeComponent(Actor* actor, const std::string& type_name)
{
    const std::string key = "r" + std::to_string(runtime_component_counter++);

    if (type_name == "Rigidbody")
    {
        Rigidbody* rb = Rigidbody::NewInstance();
        rb->key = key;
        rb->actor = actor;
        luabridge::LuaRef instance = rigidbody_to_lua_ref(lua_state, rb);
        auto instance_ptr = std::make_shared<luabridge::LuaRef>(instance);
        actor->components[key] = instance_ptr;
        actor->RebuildTypeComponentIndex();
        actor->InjectConvenienceReferences(instance_ptr);
        Actor::EnqueuePendingOnStartComponent(actor, instance_ptr);
        return instance;
    }

    if (type_name == "ParticleSystem")
    {
        ParticleSystem* ps = ParticleSystem::NewInstance();
        ps->key   = key;
        ps->actor = actor;
        luabridge::LuaRef instance = particle_system_to_lua_ref(lua_state, ps);
        auto instance_ptr = std::make_shared<luabridge::LuaRef>(instance);
        actor->components[key] = instance_ptr;
        actor->RebuildTypeComponentIndex();
        actor->InjectConvenienceReferences(instance_ptr);
        Actor::EnqueuePendingOnStartComponent(actor, instance_ptr);
        return instance;
    }

    std::shared_ptr<luabridge::LuaRef> base_table = GetComponentType(type_name);
    luabridge::LuaRef instance = luabridge::newTable(lua_state);
    instance["key"] = key;
    instance["type"] = type_name;
    instance["enabled"] = true;
    EstablishInheritance(instance, *base_table);
    auto instance_ptr = std::make_shared<luabridge::LuaRef>(instance);
    actor->components[key] = instance_ptr;
    actor->RebuildTypeComponentIndex();
    actor->InjectConvenienceReferences(instance_ptr);
    Actor::EnqueuePendingOnStartComponent(actor, instance_ptr);
    return instance;
}

void ComponentDB::ApplyActorJson(Actor& actor, const rapidjson::Value& actor_json) const
{
    if (actor_json.HasMember("name") && actor_json["name"].IsString())
    {
        actor.actor_name = actor_json["name"].GetString();
    }
    if (!actor_json.HasMember("components") || !actor_json["components"].IsObject())
    {
        return;
    }
    const rapidjson::Value& components_json = actor_json["components"];
    std::vector<std::string> component_keys;
    component_keys.reserve(components_json.MemberCount());
    for (auto it = components_json.MemberBegin(); it != components_json.MemberEnd(); ++it)
    {
        component_keys.push_back(it->name.GetString());
    }
    std::sort(component_keys.begin(), component_keys.end());
    for (const std::string& component_key : component_keys)
    {
        const rapidjson::Value& component_json = components_json[component_key.c_str()];
        if (!component_json.IsObject())
        {
            continue;
        }
        auto existing_it = actor.components.find(component_key);
        if (existing_it != actor.components.end())
        {
            luabridge::LuaRef& existing_ref = *existing_it->second;
            if (existing_ref.isUserdata())
            {
                // Determine which C++ component type this is.
                luabridge::LuaRef type_ref = existing_ref["type"];
                const std::string cpp_type = type_ref.isString() ? type_ref.cast<std::string>() : "";

                if (cpp_type == "ParticleSystem")
                {
                    ParticleSystem* parent_component = existing_ref.cast<ParticleSystem*>();
                    ParticleSystem* new_component = ParticleSystem::CloneInstance(parent_component);
                    new_component->key = component_key;
                    apply_particle_system_json(new_component, component_json);
                    luabridge::LuaRef override_instance = particle_system_to_lua_ref(lua_state, new_component);
                    existing_it->second = std::make_shared<luabridge::LuaRef>(std::move(override_instance));
                    actor.InjectConvenienceReferences(existing_it->second);
                }
                else
                {
                    // Default: treat as Rigidbody.
                    Rigidbody* parent_component = existing_ref.cast<Rigidbody*>();
                    Rigidbody* new_component = Rigidbody::CloneInstance(parent_component);
                    new_component->key = component_key;
                    apply_rigidbody_json(new_component, component_json);
                    luabridge::LuaRef override_instance = rigidbody_to_lua_ref(lua_state, new_component);
                    existing_it->second = std::make_shared<luabridge::LuaRef>(std::move(override_instance));
                    actor.InjectConvenienceReferences(existing_it->second);
                }
            }
            else
            {
                // Override mode: this key was inherited from a template.
                // Create a child instance that delegates to the existing component via __index.
                luabridge::LuaRef override_instance = luabridge::newTable(lua_state);
                override_instance["key"] = component_key;
                // Inject override properties (skip "type" — already inherited)
                for (auto it = component_json.MemberBegin(); it != component_json.MemberEnd(); ++it)
                {
                    const std::string prop_name = it->name.GetString();
                    if (prop_name == "type") continue;
                    populate_lua_value(lua_state, override_instance, prop_name, it->value);
                }
                // Chain: override_instance --> template_component_instance --> Lua class table
                EstablishInheritance(override_instance, *existing_it->second);
                existing_it->second = std::make_shared<luabridge::LuaRef>(std::move(override_instance));
                actor.InjectConvenienceReferences(existing_it->second);
            }
        }
        else
        {
            // New component: must have a "type" field
            if (!component_json.HasMember("type") || !component_json["type"].IsString())
            {
                continue;
            }
            actor.components[component_key] = std::make_shared<luabridge::LuaRef>(CreateComponentInstance(component_key, component_json));
            actor.InjectConvenienceReferences(actor.components[component_key]);
        }
    }
    actor.RebuildLifecycleComponentCaches();
}
