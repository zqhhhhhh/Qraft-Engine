#pragma once

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <unordered_set>
#include "engine/Actor.h"
#include "engine/ComponentDB.h"
#include "engine/TemplateDB.h"

class SceneDB 
{
public:
    SceneDB() = default;
    static void FlushPendingQueues();

    bool LoadScene(const std::string& scene_name, const TemplateDB& template_db, const ComponentDB& component_db);
    bool ProcessPendingSceneLoad(const TemplateDB& template_db, const ComponentDB& component_db);
    void Clear() { actors.clear(); }

    /* Lua API */
    static luabridge::LuaRef FindActorByName(const std::string& name);
    static luabridge::LuaRef FindActorsByName(const std::string& name);
    static luabridge::LuaRef InstantiateActor(const std::string& template_name);
    static void DestroyActor(Actor* actor);
    static void LuaLoadScene(const std::string& scene_name);
    static std::string LuaGetCurrentScene();
    static void LuaDontDestroyOnLoad(Actor* actor);

    /* Pending Queues */
    static std::vector<std::shared_ptr<Actor>> ConsumePendingInstantiatedActors();
    static std::vector<Actor*> ConsumePendingDestroyedActors();
    static void RemovePendingInstantiatedActors(const std::unordered_set<Actor*>& destroyed_set);

    std::vector<std::shared_ptr<Actor>>& GetActors() { return actors; }

private:
    static SceneDB* active_scene;
    static const TemplateDB* active_template_db;

    static std::vector<std::shared_ptr<Actor>> pending_instantiated_actors;
    static std::vector<Actor*> pending_destroyed_actors;
    static std::unordered_set<Actor*> pending_destroyed_actor_set;
    static std::optional<std::string> pending_scene_name;

    std::vector<std::shared_ptr<Actor>> actors;
    std::string current_scene_name;
};