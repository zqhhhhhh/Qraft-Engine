#include <algorithm>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <optional>
#include <utility>
#include <unordered_set>
#include "engine/Engine.h"
#include "engine/utils/JsonUtils.h"
#include "physics/Rigidbody.h"
#include "autograder/Helper.h"
#include "engine/Event.h"

void ReportError(const std::string &actor_name, const luabridge::LuaException &e)
{
    std::string error_message = e.what();
    /* Normalize file paths across platforms */
    std::replace(error_message.begin(), error_message.end(), '\\', '/');
    /* Display (with color codes) */
    std::cout << "\033[31m" << actor_name << " : " << error_message << "\033[0m" << std::endl;
}

namespace
{
    std::string GetPendingRemovalKey(const std::shared_ptr<luabridge::LuaRef>& component)
    {
        if (!component)
        {
            return "";
        }
        luabridge::LuaRef key_ref = (*component)["key"];
        if (!key_ref.isString())
        {
            return "";
        }
        return key_ref.cast<std::string>();
    }

    std::optional<std::filesystem::path> ResourcesRootFromSceneFile(const std::string& scene_file_hint)
    {
        if (scene_file_hint.empty())
        {
            return std::nullopt;
        }
        std::filesystem::path scene_path(scene_file_hint);
        std::error_code ec;
        scene_path = std::filesystem::absolute(scene_path, ec);
        if (ec || !std::filesystem::exists(scene_path))
        {
            return std::nullopt;
        }
        std::filesystem::path cursor = scene_path.parent_path();
        while (!cursor.empty())
        {
            if (cursor.filename() == "resources" && std::filesystem::exists(cursor / "game.config"))
            {
                return cursor;
            }
            const std::filesystem::path next = cursor.parent_path();
            if (next == cursor)
            {
                break;
            }
            cursor = next;
        }
        return std::nullopt;
    }

    std::optional<std::filesystem::path> FindNearestResourcesFromCwd()
    {
        std::filesystem::path cursor = std::filesystem::current_path();
        while (!cursor.empty())
        {
            const std::filesystem::path candidate = cursor / "resources";
            if (std::filesystem::exists(candidate / "game.config"))
            {
                return candidate;
            }
            const std::filesystem::path next = cursor.parent_path();
            if (next == cursor)
            {
                break;
            }
            cursor = next;
        }
        return std::nullopt;
    }

    std::filesystem::path ResolveResourcesRoot(const std::string& resources_root_hint, const std::string& scene_file_hint)
    {
        if (!resources_root_hint.empty())
        {
            std::filesystem::path p(resources_root_hint);
            std::error_code ec;
            p = std::filesystem::absolute(p, ec);
            if (!ec && std::filesystem::exists(p / "game.config"))
            {
                return p;
            }
        }

        if (std::optional<std::filesystem::path> from_scene = ResourcesRootFromSceneFile(scene_file_hint))
        {
            return *from_scene;
        }

        if (const char* env_root = std::getenv("GAME_ENGINE_RESOURCES_ROOT"))
        {
            std::filesystem::path p(env_root);
            std::error_code ec;
            p = std::filesystem::absolute(p, ec);
            if (!ec && std::filesystem::exists(p / "game.config"))
            {
                return p;
            }
        }

        if (const char* env_scene = std::getenv("GAME_ENGINE_SCENE_FILE"))
        {
            if (std::optional<std::filesystem::path> from_env_scene = ResourcesRootFromSceneFile(env_scene))
            {
                return *from_env_scene;
            }
        }

        if (std::optional<std::filesystem::path> nearest = FindNearestResourcesFromCwd())
        {
            return *nearest;
        }

        return std::filesystem::current_path() / "resources";
    }
}

void Engine::ProcessAddedComponents()
{
    std::vector<Actor::PendingOnStartComponent> runtime_pending = Actor::ConsumePendingOnStartComponents();
    for (const auto& pending : runtime_pending)
    {
        Actor* actor = pending.actor;
        const std::shared_ptr<luabridge::LuaRef>& comp_ptr = pending.component;
        if (!actor || !comp_ptr)
        {
            continue;
        }
        luabridge::LuaRef key_ref = (*comp_ptr)["key"];
        if (!key_ref.isString())
        {
            continue;
        }
        const std::string key = key_ref.cast<std::string>();
        auto in_actor = actor->components.find(key);
        if (in_actor == actor->components.end())
        {
            continue;
        }
        luabridge::LuaRef enabled_ref = (*comp_ptr)["enabled"];
        bool is_enabled = !(enabled_ref.isBool() && !enabled_ref.cast<bool>());
        if (is_enabled)
        {
            luabridge::LuaRef on_start = (*comp_ptr)["OnStart"];
            if (on_start.isFunction())
            {
                try { on_start(*comp_ptr); }
                catch (const luabridge::LuaException& e) { ReportError(actor->GetName(), e); }
            }
        }
        // Add to lifecycle caches regardless of enabled state
        luabridge::LuaRef on_update = (*comp_ptr)["OnUpdate"];
        if (on_update.isFunction()) actor->components_requiring_onupdate[key] = comp_ptr;
        luabridge::LuaRef on_late_update = (*comp_ptr)["OnLateUpdate"];
        if (on_late_update.isFunction()) actor->components_requiring_onlateupdate[key] = comp_ptr;
        luabridge::LuaRef on_destroy = (*comp_ptr)["OnDestroy"];
        if (on_destroy.isFunction()) actor->components_requiring_ondestroy[key] = comp_ptr;
        luabridge::LuaRef on_collision_enter = (*comp_ptr)["OnCollisionEnter"];
        if (on_collision_enter.isFunction()) actor->components_requiring_oncollisionenter[key] = comp_ptr;
        luabridge::LuaRef on_collision_exit = (*comp_ptr)["OnCollisionExit"];
        if (on_collision_exit.isFunction()) actor->components_requiring_oncollisionexit[key] = comp_ptr;
        luabridge::LuaRef on_trigger_enter = (*comp_ptr)["OnTriggerEnter"];
        if (on_trigger_enter.isFunction()) actor->components_requiring_ontriggerenter[key] = comp_ptr;
        luabridge::LuaRef on_trigger_exit = (*comp_ptr)["OnTriggerExit"];
        if (on_trigger_exit.isFunction()) actor->components_requiring_ontriggerexit[key] = comp_ptr;
    }
}

void Engine::ProcessOnStartQueue()
{
    std::vector<std::shared_ptr<Actor>> actors_to_start = SceneDB::ConsumePendingInstantiatedActors();
    if (actors_to_start.empty())
    {
        return;
    }

    std::vector<std::shared_ptr<Actor>>& scene_actors = scene_db.GetActors();
    scene_actors.reserve(scene_actors.size() + actors_to_start.size());
    for (const auto& actor : actors_to_start)
    {
        if (!actor)
        {
            continue;
        }
        scene_actors.push_back(actor);
    }
    for (const auto& actor : actors_to_start)
    {
        if (!actor)
        {
            continue;
        }
        for (const auto& [component_key, component_ref_ptr] : actor->components_requiring_onstart)
        {
            (void)component_key;
            if (!component_ref_ptr)
            {
                continue;
            }
            luabridge::LuaRef enabled_ref = (*component_ref_ptr)["enabled"];
            bool is_enabled = !(enabled_ref.isBool() && !enabled_ref.cast<bool>());
            if (!is_enabled) continue;
            luabridge::LuaRef on_start = (*component_ref_ptr)["OnStart"];
            if (on_start.isFunction())
            {
                try
                {
                    on_start(*component_ref_ptr);
                }
                catch (const luabridge::LuaException& e)
                {
                    ReportError(actor->GetName(), e);
                }
            }
        }
    }
}

void Engine::ProcessPendingComponentRemovals()
{
    std::vector<Actor::PendingRemovalComponent> pending_removals = Actor::ConsumePendingRemovalComponents();
    std::stable_sort(pending_removals.begin(), pending_removals.end(),
        [](const Actor::PendingRemovalComponent& a, const Actor::PendingRemovalComponent& b)
        {
            if (a.actor != b.actor)
            {
                return false;  // preserve enqueue order across different actors
            }
            return GetPendingRemovalKey(a.component) < GetPendingRemovalKey(b.component);
        });
    for (const auto& pending : pending_removals)
    {
        Actor* actor = pending.actor;
        const std::shared_ptr<luabridge::LuaRef>& component_ref_ptr = pending.component;
        if (!actor || !component_ref_ptr)
        {
            continue;
        }
        luabridge::LuaRef key_ref = (*component_ref_ptr)["key"];
        if (!key_ref.isString())
        {
            continue;
        }
        const std::string key = key_ref.cast<std::string>();
        Actor::InvokeOnDestroy(actor, component_ref_ptr);
        actor->components_requiring_onstart.erase(key);
        actor->components_requiring_onupdate.erase(key);
        actor->components_requiring_onlateupdate.erase(key);
        actor->components_requiring_ondestroy.erase(key);
        actor->components_requiring_oncollisionenter.erase(key);
        actor->components_requiring_oncollisionexit.erase(key);
        actor->components_requiring_ontriggerenter.erase(key);
        actor->components_requiring_ontriggerexit.erase(key);
    }
}

void Engine::ProcessDestroyedActors()
{
    std::vector<Actor*> destroyed_actors = SceneDB::ConsumePendingDestroyedActors();
    if (destroyed_actors.empty())
    {
        return;
    }
    std::unordered_set<Actor*> destroyed_set(destroyed_actors.begin(), destroyed_actors.end());
    SceneDB::RemovePendingInstantiatedActors(destroyed_set);
    std::vector<std::shared_ptr<Actor>>& scene_actors = scene_db.GetActors();
    scene_actors.erase(
        std::remove_if(scene_actors.begin(), scene_actors.end(),
            [&destroyed_set](const std::shared_ptr<Actor>& actor)
            {
                return actor && destroyed_set.find(actor.get()) != destroyed_set.end();
            }),
        scene_actors.end());
}

void Engine::ProcessOnUpdateCalls()
{
    const std::vector<std::shared_ptr<Actor>>& actors = scene_db.GetActors();
    for (const auto& actor : actors)
    {
        if (!actor) continue;
        for (const auto& [component_key, component_ref_ptr] : actor->components_requiring_onupdate)
        {
            (void)component_key;
            if (!component_ref_ptr) continue;
            luabridge::LuaRef enabled_ref = (*component_ref_ptr)["enabled"];
            bool is_enabled = !(enabled_ref.isBool() && !enabled_ref.cast<bool>());
            if (!is_enabled) continue;
            luabridge::LuaRef on_update = (*component_ref_ptr)["OnUpdate"];
            if (on_update.isFunction())
            {
                try
                {
                    on_update(*component_ref_ptr);
                }
                catch (const luabridge::LuaException& e)
                {
                    ReportError(actor->GetName(), e);
                }
            }
        }
    }
}

void Engine::ProcessOnLateUpdateCalls()
{
    const std::vector<std::shared_ptr<Actor>>& actors = scene_db.GetActors();
    for (const auto& actor : actors)
    {
        if (!actor) continue;
        for (const auto& [component_key, component_ref_ptr] : actor->components_requiring_onlateupdate)
        {
            (void)component_key;
            if (!component_ref_ptr) continue;
            luabridge::LuaRef enabled_ref = (*component_ref_ptr)["enabled"];
            bool is_enabled = !(enabled_ref.isBool() && !enabled_ref.cast<bool>());
            if (!is_enabled) continue;
            luabridge::LuaRef on_late_update = (*component_ref_ptr)["OnLateUpdate"];
            if (on_late_update.isFunction())
            {
                try
                {
                    on_late_update(*component_ref_ptr);
                }
                catch (const luabridge::LuaException& e)
                {
                    ReportError(actor->GetName(), e);
                }
            }
        }
    }
}

Engine::Engine(const std::string& resources_root_hint, const std::string& scene_file_hint)
{
    const std::filesystem::path resources_root = ResolveResourcesRoot(resources_root_hint, scene_file_hint);
    const std::filesystem::path project_root = resources_root.parent_path();
    if (!project_root.empty())
    {
        std::error_code ec;
        std::filesystem::current_path(project_root, ec);
        if (ec)
        {
            std::cout << "warning: failed to switch working directory to " << project_root.string() << std::endl;
        }
    }

    // ---- Ensure a minimal resources layout exists so the editor can open with no project ----
    if (!std::filesystem::exists("resources"))
    {
        std::filesystem::create_directories("resources/scenes");
        std::filesystem::create_directories("resources/actor_templates");
        std::filesystem::create_directories("resources/component_types");
        std::ofstream cfg("resources/game.config");
        cfg << R"({"game_title": "New Project", "initial_scene": "scene0"})";
        cfg.close();
        std::ofstream sf("resources/scenes/scene0.scene");
        sf << R"({"actors": []})";
        sf.close();
    }
    else if (!std::filesystem::exists("resources/game.config"))
    {
        std::ofstream cfg("resources/game.config");
        cfg << R"({"game_title": "New Project", "initial_scene": "scene0"})";
        cfg.close();
    }

    JsonUtils::ReadJsonFile("resources/game.config", game_config_doc);
    renderer.Config(game_config_doc);

    component_db.Initialize();
    template_db.SetComponentDB(&component_db);
    template_db.LoadTemplates("resources/actor_templates");

    // Determine initial scene; default to "scene0" if not specified
    std::string initial_scene = "scene0";
    if (game_config_doc.HasMember("initial_scene") && game_config_doc["initial_scene"].IsString())
    {
        initial_scene = game_config_doc["initial_scene"].GetString();
    }

    // Create an empty scene file if it does not already exist
    const std::string scene_path = "resources/scenes/" + initial_scene + ".scene";
    if (!std::filesystem::exists(scene_path))
    {
        std::filesystem::create_directories("resources/scenes");
        std::ofstream sf(scene_path);
        sf << R"({"actors": []})";
        sf.close();
    }

    scene_db.LoadScene(initial_scene, template_db, component_db);

    // Initialize scene editor
    scene_editor = std::make_unique<SceneEditor>();
    scene_editor->Initialize(&scene_db, &template_db, &component_db);
}

bool Engine::LoadProjectFromResourcesRoot(const std::string& resources_root_path, std::string& out_message)
{
    std::filesystem::path resources_root(resources_root_path);
    std::error_code ec;
    resources_root = std::filesystem::absolute(resources_root, ec);
    if (ec)
    {
        out_message = "Invalid path";
        return false;
    }

    resources_root = resources_root.lexically_normal();
    if (resources_root.filename().empty())
    {
        resources_root = resources_root.parent_path();
    }

    if (!std::filesystem::exists(resources_root) || resources_root.filename() != "resources")
    {
        out_message = "Selected folder must be a resources folder";
        return false;
    }
    if (!std::filesystem::exists(resources_root / "game.config"))
    {
        out_message = "Invalid resources folder: game.config missing";
        return false;
    }

    const std::filesystem::path project_root = resources_root.parent_path();
    std::filesystem::current_path(project_root, ec);
    if (ec)
    {
        out_message = "Failed to switch project directory";
        return false;
    }

    std::filesystem::create_directories("resources/scenes");
    std::filesystem::create_directories("resources/component_types");
    std::filesystem::create_directories("resources/actor_templates");
    std::filesystem::create_directories("resources/images");

    try
    {
        JsonUtils::ReadJsonFile("resources/game.config", game_config_doc);
    }
    catch (...)
    {
        out_message = "Failed to parse game.config";
        return false;
    }

    renderer.Config(game_config_doc);
    component_db.ClearComponentTypeCache();
    template_db.SetComponentDB(&component_db);
    template_db.LoadTemplates("resources/actor_templates");

    std::string initial_scene = "scene0";
    if (game_config_doc.HasMember("initial_scene") && game_config_doc["initial_scene"].IsString())
    {
        initial_scene = game_config_doc["initial_scene"].GetString();
    }

    const std::string scene_path = "resources/scenes/" + initial_scene + ".scene";
    if (!std::filesystem::exists(scene_path))
    {
        std::ofstream sf(scene_path);
        sf << R"({"actors": []})";
        sf.close();
    }

    Actor::FlushPendingQueues();
    SceneDB::FlushPendingQueues();
    if (!scene_db.LoadScene(initial_scene, template_db, component_db))
    {
        out_message = "Failed to load initial scene";
        return false;
    }

    if (scene_editor)
    {
        scene_editor->OnProjectLoaded();
    }

    out_message = "Project loaded: " + resources_root.string();
    return true;
}

void Engine::GameLoop()
{
    if (!renderer.Init(game_config_doc))
    {
        std::cout << "Failed to initialize renderer." << std::endl;
        exit(0);
    }
    while (!saw_quit_event)
    {
        Input();
        if (saw_quit_event)
        {
            break;
        }

        if (scene_editor && scene_editor->HasPendingProjectLoadRequest())
        {
            const std::string requested_resources = scene_editor->ConsumePendingProjectLoadResourcesPath();
            std::string load_message;
            const bool loaded = LoadProjectFromResourcesRoot(requested_resources, load_message);
            scene_editor->NotifyProjectLoadResult(loaded, load_message);
        }

        // Only advance game simulation when the Play button is active
        if (scene_editor && scene_editor->IsPlaying())
        {
            Update();
        }
        else
        {
            // In editor mode, still process scene/component queues so hierarchy stays in sync.
            ProcessPendingSceneLoad();
            ProcessAddedComponents();
            ProcessOnStartQueue();
            ProcessPendingComponentRemovals();
            ProcessDestroyedActors();
            EventBus::ProcessPendingSubscriptions();
            input.LateUpdate();
        }
        Render();
    }
}

void Engine::Input()
{
    SDL_Event e;
    while (Helper::SDL_PollEvent(&e))
    {
        if (scene_editor)
        {
            scene_editor->ProcessEvent(e);
        }
        input.ProcessEvent(e);
        if (e.type == SDL_QUIT)
        {
            saw_quit_event = true;
        }
    }
    // Let the editor's Quit button also exit the engine
    if (scene_editor && scene_editor->ShouldQuit())
    {
        saw_quit_event = true;
    }
}

void Engine::ProcessPendingSceneLoad()
{
    if (!scene_db.ProcessPendingSceneLoad(template_db, component_db))
    {
        exit(0);
    }
}

void Engine::Update()
{
    ProcessPendingSceneLoad();
    ProcessAddedComponents();
    ProcessOnStartQueue();
    ProcessOnUpdateCalls();
    ProcessOnLateUpdateCalls();
    ProcessPendingComponentRemovals();
    ProcessDestroyedActors();
    EventBus::ProcessPendingSubscriptions();
    Rigidbody::StepWorld();
    Rigidbody::DispatchCollisionEvents();
    input.LateUpdate();
}

void Engine::Render()
{
    SDL_Renderer* sdl_renderer = renderer.GetSDLRenderer();
    SDL_Rect game_viewport{ 0, 0, 0, 0 };

    if (sdl_renderer)
    {
        SDL_RenderSetViewport(sdl_renderer, nullptr);
    }

    if (scene_editor && !scene_editor->IsPlaying())
    {
        scene_editor->EnqueueEditorPreviewDrawRequests();
    }

    if (scene_editor && sdl_renderer)
    {
        game_viewport = scene_editor->GetGameViewportRect(renderer.GetSDLWindow());

        // Now restrict rendering to the exact game resolution area.
        SDL_RenderSetViewport(sdl_renderer, &game_viewport);
    }

    renderer.Clear();

    renderer.RenderAndClearAllImageRequests();
    renderer.RenderAndClearAllUI();
    renderer.RenderAndClearAllTextRequests();
    renderer.RenderAndClearAllPixels();

    if (sdl_renderer)
    {
        SDL_RenderSetViewport(sdl_renderer, nullptr);
    }

    // Draw gray letterbox AFTER game content, with full-window viewport, BEFORE ImGui.
    // Fills the 4 strips around the game rect inside the center area.
    if (scene_editor && sdl_renderer && !scene_editor->IsShowingProjectHub())
    {
        const SDL_Rect center = scene_editor->GetEditorCenterAreaRect(renderer.GetSDLWindow());
        // Top strip
        SDL_Rect top    = { center.x, center.y, center.w, game_viewport.y - center.y };
        // Bottom strip
        SDL_Rect bottom = { center.x, game_viewport.y + game_viewport.h,
                            center.w, (center.y + center.h) - (game_viewport.y + game_viewport.h) };
        // Left strip
        SDL_Rect left   = { center.x, game_viewport.y,
                            game_viewport.x - center.x, game_viewport.h };
        // Right strip
        SDL_Rect right  = { game_viewport.x + game_viewport.w, game_viewport.y,
                            (center.x + center.w) - (game_viewport.x + game_viewport.w), game_viewport.h };
        SDL_SetRenderDrawColor(sdl_renderer, 40, 40, 40, 255);
        if (top.h    > 0) SDL_RenderFillRect(sdl_renderer, &top);
        if (bottom.h > 0) SDL_RenderFillRect(sdl_renderer, &bottom);
        if (left.w   > 0) SDL_RenderFillRect(sdl_renderer, &left);
        if (right.w  > 0) SDL_RenderFillRect(sdl_renderer, &right);
    }

    if (scene_editor && sdl_renderer)
    {
        if (scene_editor->IsPlaying())
        {
            scene_editor->CaptureGameFrame(sdl_renderer, game_viewport);
        }
    }

    if (scene_editor)
    {
        scene_editor->RenderFrame(renderer.GetSDLWindow(), renderer.GetSDLRenderer());
    }

    renderer.Present();
}

Engine::~Engine()
{
    if (scene_editor)
    {
        scene_editor.reset();
    }
    scene_db.Clear();
    Actor::FlushPendingQueues();
    SceneDB::FlushPendingQueues();
    component_db.Shutdown();
}