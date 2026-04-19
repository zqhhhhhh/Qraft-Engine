#include "engine/Actor.h"
#include "engine/ComponentDB.h"
#include <iostream>

Actor::ActorID Actor::next_id = 1; // Initialize static member once
std::vector<Actor::PendingOnStartComponent> Actor::pending_on_start_components;
std::vector<Actor::PendingRemovalComponent> Actor::pending_removal_components;

void Actor::EnqueuePendingOnStartComponent(Actor* actor, const std::shared_ptr<luabridge::LuaRef>& component)
{
    if (!actor || !component)
    {
        return;
    }
    pending_on_start_components.push_back({actor, component});
}

std::vector<Actor::PendingOnStartComponent> Actor::ConsumePendingOnStartComponents()
{
    std::vector<PendingOnStartComponent> pending;
    pending.swap(pending_on_start_components);
    return pending;
}

void Actor::EnqueuePendingRemovalComponent(Actor* actor, const std::shared_ptr<luabridge::LuaRef>& component)
{
    if (!actor || !component)
    {
        return;
    }
    pending_removal_components.push_back({actor, component});
}

std::vector<Actor::PendingRemovalComponent> Actor::ConsumePendingRemovalComponents()
{
    std::vector<PendingRemovalComponent> pending;
    pending.swap(pending_removal_components);
    return pending;
}

Actor::Actor()
        : id(next_id++), actor_name(""), components(),
            dont_destroy_on_load(false),
            components_by_type(), first_component_by_type(),
            components_requiring_onstart(), components_requiring_onupdate(), components_requiring_onlateupdate(), components_requiring_ondestroy(),
            components_requiring_oncollisionenter(), components_requiring_oncollisionexit(),
            components_requiring_ontriggerenter(), components_requiring_ontriggerexit() {}

Actor::Actor(const Actor& other) : id(next_id++), // New unique ID
    actor_name(other.actor_name),
        dont_destroy_on_load(other.dont_destroy_on_load),
        components(other.components),
        components_by_type(other.components_by_type),
        first_component_by_type(other.first_component_by_type),
        components_requiring_onstart(other.components_requiring_onstart),
        components_requiring_onupdate(other.components_requiring_onupdate),
        components_requiring_onlateupdate(other.components_requiring_onlateupdate),
        components_requiring_ondestroy(other.components_requiring_ondestroy),
        components_requiring_oncollisionenter(other.components_requiring_oncollisionenter),
        components_requiring_oncollisionexit(other.components_requiring_oncollisionexit),
        components_requiring_ontriggerenter(other.components_requiring_ontriggerenter),
        components_requiring_ontriggerexit(other.components_requiring_ontriggerexit)
    {}

void Actor::InjectConvenienceReferences(std::shared_ptr<luabridge::LuaRef> component_ref)
{
    (*component_ref)["actor"] = this;
}

Actor::ActorID Actor::GetID() const 
{ 
    return id; 
}

std::string Actor::GetName() const
{
    return actor_name;
}

luabridge::LuaRef Actor::GetComponentByKey(const std::string& key) const
{
    auto it = components.find(key);
    if (it != components.end() && it->second)
    {
        return *(it->second);
    }
    return luabridge::LuaRef(ComponentDB::GetLuaState());
}

luabridge::LuaRef Actor::GetComponent(const std::string& type_name) const
{
    auto found = first_component_by_type.find(type_name);
    if (found != first_component_by_type.end() && found->second)
    {
        return *(found->second);
    }
    return luabridge::LuaRef(ComponentDB::GetLuaState());
}

luabridge::LuaRef Actor::GetComponents(const std::string& type_name) const
{
    lua_State* L = ComponentDB::GetLuaState();
    if (L == nullptr)
    {
        std::cout << "error: lua state unavailable";
        exit(0);
    }
    luabridge::LuaRef result = luabridge::newTable(L);
    int table_index = 1;
    auto found = components_by_type.find(type_name);
    if (found == components_by_type.end())
    {
        return result;
    }
    for (const auto& component_ref_ptr : found->second)
    {
        if (component_ref_ptr)
        {
            result[table_index] = *component_ref_ptr;
            ++table_index;
        }
    }
    return result;
}

luabridge::LuaRef Actor::AddComponent(const std::string& type_name)
{
    return ComponentDB::active_instance->CreateRuntimeComponent(this, type_name);
}

void Actor::RemoveComponent(const luabridge::LuaRef& component_ref)
{
    if (component_ref.isNil())
    {
        return;
    }
    luabridge::LuaRef key_ref = component_ref["key"];
    if (!key_ref.isString())
    {
        return;
    }
    const std::string component_key = key_ref.cast<std::string>();
    component_ref["enabled"] = false;
    auto found = components.find(component_key);
    if (found != components.end() && found->second)
    {
        (*(found->second))["enabled"] = false;
        EnqueuePendingRemovalComponent(this, found->second);
    }
    components.erase(component_key);
    RebuildTypeComponentIndex();
}

void Actor::InvokeOnDestroy(Actor* actor, const std::shared_ptr<luabridge::LuaRef>& component_ref)
{
    if (actor == nullptr || !component_ref)
    {
        return;
    }
    luabridge::LuaRef on_destroy = (*component_ref)["OnDestroy"];
    if (!on_destroy.isFunction())
    {
        return;
    }
    try
    {
        on_destroy(*component_ref);
    }
    catch (const luabridge::LuaException& e)
    {
        std::cout << "\033[31m" << actor->GetName() << " : " << e.what() << "\033[0m" << std::endl;
    }
}

void Actor::RebuildTypeComponentIndex()
{
    components_by_type.clear();
    first_component_by_type.clear();
    for (const auto& [component_key, component_ref_ptr] : components)
    {
        (void)component_key;
        if (!component_ref_ptr)
        {
            continue;
        }
        luabridge::LuaRef type_ref = (*component_ref_ptr)["type"];
        if (!type_ref.isString())
        {
            continue;
        }
        const std::string type_name = type_ref.cast<std::string>();
        auto& bucket = components_by_type[type_name];
        if (bucket.empty())
        {
            first_component_by_type[type_name] = component_ref_ptr;
        }
        bucket.push_back(component_ref_ptr);
    }
}

void Actor::RebuildLifecycleComponentCaches()
{
    RebuildTypeComponentIndex();
    components_requiring_onstart.clear();
    components_requiring_onupdate.clear();
    components_requiring_onlateupdate.clear();
    components_requiring_ondestroy.clear();
    components_requiring_oncollisionenter.clear();
    components_requiring_oncollisionexit.clear();
    components_requiring_ontriggerenter.clear();
    components_requiring_ontriggerexit.clear();
    for (const auto& [component_key, component_ref_ptr] : components)
    {
        if (!component_ref_ptr)
        {
            continue;
        }
        luabridge::LuaRef on_start = (*component_ref_ptr)["OnStart"];
        if (on_start.isFunction())
        {
            components_requiring_onstart[component_key] = component_ref_ptr;
        }
        luabridge::LuaRef on_update = (*component_ref_ptr)["OnUpdate"];
        if (on_update.isFunction())
        {
            components_requiring_onupdate[component_key] = component_ref_ptr;
        }
        luabridge::LuaRef on_late_update = (*component_ref_ptr)["OnLateUpdate"];
        if (on_late_update.isFunction())
        {
            components_requiring_onlateupdate[component_key] = component_ref_ptr;
        }
        luabridge::LuaRef on_destroy = (*component_ref_ptr)["OnDestroy"];
        if (on_destroy.isFunction())
        {
            components_requiring_ondestroy[component_key] = component_ref_ptr;
        }
        luabridge::LuaRef on_collision_enter = (*component_ref_ptr)["OnCollisionEnter"];
        if (on_collision_enter.isFunction())
        {
            components_requiring_oncollisionenter[component_key] = component_ref_ptr;
        }
        luabridge::LuaRef on_collision_exit = (*component_ref_ptr)["OnCollisionExit"];
        if (on_collision_exit.isFunction())
        {
            components_requiring_oncollisionexit[component_key] = component_ref_ptr;
        }
        luabridge::LuaRef on_trigger_enter = (*component_ref_ptr)["OnTriggerEnter"];
        if (on_trigger_enter.isFunction())
        {
            components_requiring_ontriggerenter[component_key] = component_ref_ptr;
        }
        luabridge::LuaRef on_trigger_exit = (*component_ref_ptr)["OnTriggerExit"];
        if (on_trigger_exit.isFunction())
        {
            components_requiring_ontriggerexit[component_key] = component_ref_ptr;
        }
    }
}

void Actor::FlushPendingQueues()
{
    pending_on_start_components.clear();
    pending_removal_components.clear();
}
