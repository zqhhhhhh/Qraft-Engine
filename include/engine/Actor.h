#pragma once
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <unordered_set>
#include <optional>
#include "lua/lua.hpp"
#include "LuaBridge/LuaBridge.h"
#include "rapidjson/document.h"

class Actor
{
public:
    using ActorID = uint64_t;
	struct PendingOnStartComponent
	{
		Actor* actor;
		std::shared_ptr<luabridge::LuaRef> component;
	};
	struct PendingRemovalComponent
	{
		Actor* actor;
		std::shared_ptr<luabridge::LuaRef> component;
	};
	static void EnqueuePendingOnStartComponent(Actor* actor, const std::shared_ptr<luabridge::LuaRef>& component);
	static std::vector<PendingOnStartComponent> ConsumePendingOnStartComponents();
	static void EnqueuePendingRemovalComponent(Actor* actor, const std::shared_ptr<luabridge::LuaRef>& component);
	static std::vector<PendingRemovalComponent> ConsumePendingRemovalComponents();
private:
    ActorID id;
    static ActorID next_id;
	static std::vector<PendingOnStartComponent> pending_on_start_components;
	static std::vector<PendingRemovalComponent> pending_removal_components;
public:
	std::string actor_name;
	bool dont_destroy_on_load = false;
	std::map<std::string, std::shared_ptr<luabridge::LuaRef>> components;
	std::unordered_map<std::string, std::vector<std::shared_ptr<luabridge::LuaRef>>> components_by_type;
	std::unordered_map<std::string, std::shared_ptr<luabridge::LuaRef>> first_component_by_type;
	std::map<std::string, std::shared_ptr<luabridge::LuaRef>> components_requiring_onstart;
	std::map<std::string, std::shared_ptr<luabridge::LuaRef>> components_requiring_onupdate;
	std::map<std::string, std::shared_ptr<luabridge::LuaRef>> components_requiring_onlateupdate;
	std::map<std::string, std::shared_ptr<luabridge::LuaRef>> components_requiring_ondestroy;
	std::map<std::string, std::shared_ptr<luabridge::LuaRef>> components_requiring_oncollisionenter;
	std::map<std::string, std::shared_ptr<luabridge::LuaRef>> components_requiring_oncollisionexit;
	std::map<std::string, std::shared_ptr<luabridge::LuaRef>> components_requiring_ontriggerenter;
	std::map<std::string, std::shared_ptr<luabridge::LuaRef>> components_requiring_ontriggerexit;

	Actor();
	Actor(const Actor& other);
    ActorID GetID() const;
	std::string GetName() const;
	luabridge::LuaRef GetComponentByKey(const std::string& key) const;
	luabridge::LuaRef GetComponent(const std::string& type_name) const;
	luabridge::LuaRef GetComponents(const std::string& type_name) const;
	luabridge::LuaRef AddComponent(const std::string& type_name);
    void RemoveComponent(const luabridge::LuaRef& component_ref);
	static void InvokeOnDestroy(Actor* actor, const std::shared_ptr<luabridge::LuaRef>& component_ref);
	void RebuildLifecycleComponentCaches();
	void RebuildTypeComponentIndex();
	void InjectConvenienceReferences(std::shared_ptr<luabridge::LuaRef> component_ref);
	static void FlushPendingQueues();
};