#include <iostream>
#include <filesystem>
#include <algorithm>
#include "engine/SceneDB.h"
#include "engine/utils/JsonUtils.h"
#include "physics/Rigidbody.h"
// #include "engine/Timer.h"

SceneDB* SceneDB::active_scene = nullptr;
const TemplateDB* SceneDB::active_template_db = nullptr;
std::vector<std::shared_ptr<Actor>> SceneDB::pending_instantiated_actors;
std::vector<Actor*> SceneDB::pending_destroyed_actors;
std::unordered_set<Actor*> SceneDB::pending_destroyed_actor_set;
std::optional<std::string> SceneDB::pending_scene_name;

void SceneDB::LuaLoadScene(const std::string& scene_name)
{
    pending_scene_name = scene_name;
}

std::string SceneDB::LuaGetCurrentScene()
{
    if (active_scene == nullptr)
    {
        return "";
    }
    return active_scene->current_scene_name;
}

void SceneDB::LuaDontDestroyOnLoad(Actor* actor)
{
    if (actor == nullptr)
    {
        return;
    }
    actor->dont_destroy_on_load = true;
}

bool SceneDB::ProcessPendingSceneLoad(const TemplateDB& template_db, const ComponentDB& component_db)
{
    if (!pending_scene_name.has_value())
    {
        return true;
    }
    const std::string scene_name_to_load = pending_scene_name.value();
    pending_scene_name.reset();
    return LoadScene(scene_name_to_load, template_db, component_db);
}

luabridge::LuaRef SceneDB::FindActorByName(const std::string& name)
{
    lua_State* L = ComponentDB::GetLuaState();
    if (L == nullptr)
    {
        std::cout << "error: lua state unavailable";
        exit(0);
    }
    if (active_scene == nullptr)
    {
        return luabridge::LuaRef(L);
    }
    for (const auto& actor : active_scene->actors)
    {
        if (actor && pending_destroyed_actor_set.find(actor.get()) == pending_destroyed_actor_set.end() && actor->actor_name == name)
        {
            return luabridge::LuaRef(L, actor.get());
        }
    }
    for (const auto& actor : pending_instantiated_actors)
    {
        if (actor && pending_destroyed_actor_set.find(actor.get()) == pending_destroyed_actor_set.end() && actor->actor_name == name)
        {
            return luabridge::LuaRef(L, actor.get());
        }
    }
    return luabridge::LuaRef(L);
}

luabridge::LuaRef SceneDB::FindActorsByName(const std::string& name)
{
    lua_State* L = ComponentDB::GetLuaState();
    if (L == nullptr)
    {
        std::cout << "error: lua state unavailable";
        exit(0);
    }
    luabridge::LuaRef result = luabridge::newTable(L);
    if (active_scene == nullptr)
    {
        return result;
    }
    int index = 1;
    for (const auto& actor : active_scene->actors)
    {
        if (actor && pending_destroyed_actor_set.find(actor.get()) == pending_destroyed_actor_set.end() && actor->actor_name == name)
        {
            result[index] = actor.get();
            ++index;
        }
    }
    for (const auto& actor : pending_instantiated_actors)
    {
        if (actor && pending_destroyed_actor_set.find(actor.get()) == pending_destroyed_actor_set.end() && actor->actor_name == name)
        {
            result[index] = actor.get();
            ++index;
        }
    }
    return result;
}

bool SceneDB::LoadScene(const std::string& scene_name, const TemplateDB& template_db, const ComponentDB& component_db) 
{
    active_scene = this;
    active_template_db = &template_db;

    // Prevent stale collision callbacks from old scene actors.
    Rigidbody::ClearPendingCollisionEvents();

    std::vector<std::shared_ptr<Actor>> persistent_actors;
    const bool replacing_existing_scene = !current_scene_name.empty() || !actors.empty();
    persistent_actors.reserve(actors.size());
    if (replacing_existing_scene)
    {
        for (const auto& actor : actors)
        {
            if (actor && actor->dont_destroy_on_load)
            {
                persistent_actors.push_back(actor);
            }
            else if (actor)
            {
                for (const auto& [component_key, component_ref_ptr] : actor->components)
                {
                    (void)component_key;
                    if (!component_ref_ptr)
                    {
                        continue;
                    }
                    (*component_ref_ptr)["enabled"] = false;
                    Actor::InvokeOnDestroy(actor.get(), component_ref_ptr);
                }
                actor->components.clear();
            }
        }
    }
    actors.swap(persistent_actors);

    pending_instantiated_actors.clear();
    pending_destroyed_actors.clear();
    pending_destroyed_actor_set.clear();

    std::string scene_path = "resources/scenes/" + scene_name + ".scene";
    if (!std::filesystem::exists(scene_path)) 
    {
        std::cout << "error: scene " << scene_name << " is missing";
        return false;
    }
    rapidjson::Document scene_doc;
    JsonUtils::ReadJsonFile(scene_path, scene_doc);
    current_scene_name = scene_name;
    if (scene_doc.HasMember("actors")) 
    {
        size_t actor_count = scene_doc["actors"].Size();
        pending_instantiated_actors.reserve(actor_count);
        for (auto& a : scene_doc["actors"].GetArray())
        {
            std::shared_ptr<Actor> actor;
            if (a.HasMember("template")) 
            {
                std::string tmpl = a["template"].GetString();
                actor = template_db.Instantiate(tmpl);
            } 
            else 
            {
                actor = std::make_shared<Actor>();
            }
            component_db.ApplyActorJson(*actor, a);
            pending_instantiated_actors.push_back(actor);
        }
    }
    return true;
}

luabridge::LuaRef SceneDB::InstantiateActor(const std::string& template_name)
{
    lua_State* L = ComponentDB::GetLuaState();
    if (L == nullptr)
    {
        std::cout << "error: lua state unavailable";
        exit(0);
    }
    if (active_scene == nullptr || active_template_db == nullptr)
    {
        return luabridge::LuaRef(L);
    }
    std::shared_ptr<Actor> actor = active_template_db->Instantiate(template_name);
    pending_instantiated_actors.push_back(actor);
    return luabridge::LuaRef(L, actor.get());
}

void SceneDB::DestroyActor(Actor* actor)
{
    if (active_scene == nullptr || actor == nullptr)
    {
        return;
    }
    for (auto& [component_key, component_ref_ptr] : actor->components)
    {
        (void)component_key;
        if (!component_ref_ptr)
        {
            continue;
        }
        (*component_ref_ptr)["enabled"] = false;
        Actor::EnqueuePendingRemovalComponent(actor, component_ref_ptr);
    }
    actor->components.clear();
    if (pending_destroyed_actor_set.insert(actor).second)
    {
        pending_destroyed_actors.push_back(actor);
    }
}

std::vector<std::shared_ptr<Actor>> SceneDB::ConsumePendingInstantiatedActors()
{
    std::vector<std::shared_ptr<Actor>> pending;
    pending.swap(pending_instantiated_actors);
    return pending;
}

void SceneDB::RemovePendingInstantiatedActors(const std::unordered_set<Actor*>& destroyed_set)
{
    pending_instantiated_actors.erase(
        std::remove_if(pending_instantiated_actors.begin(), pending_instantiated_actors.end(),
            [&destroyed_set](const std::shared_ptr<Actor>& actor)
            {
                return actor && destroyed_set.find(actor.get()) != destroyed_set.end();
            }),
        pending_instantiated_actors.end());
}

std::vector<Actor*> SceneDB::ConsumePendingDestroyedActors()
{
    std::vector<Actor*> pending;
    pending.swap(pending_destroyed_actors);
    pending_destroyed_actor_set.clear();
    return pending;
}

void SceneDB::FlushPendingQueues()
{
    pending_instantiated_actors.clear();
    pending_destroyed_actors.clear();
    pending_destroyed_actor_set.clear();
}
