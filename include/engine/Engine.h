#pragma once

#include <vector>
#include <string>
#include <memory>
#include "rapidjson/document.h"
#include "engine/Actor.h"
#include "engine/ComponentDB.h"
#include "engine/SceneDB.h"
#include "engine/TemplateDB.h"
#include "renderer/Renderer.h"
#include "inputManager/Input.h"
#include "editor/SceneEditor.h"

class Engine 
{
    /* Game State */
    bool saw_quit_event = false;
    void Input();
    void Update();
    void Render();
    void ProcessPendingSceneLoad();
    void ProcessAddedComponents();
    void ProcessOnStartQueue();
    void ProcessOnUpdateCalls();
    void ProcessOnLateUpdateCalls();
    void ProcessPendingComponentRemovals();
    void ProcessDestroyedActors();
    bool LoadProjectFromResourcesRoot(const std::string& resources_root_path, std::string& out_message);

    rapidjson::Document game_config_doc;
    SceneDB scene_db;
    InputManager input;
    ComponentDB component_db;
    TemplateDB template_db;

    /* Renderer (2D) */
    Renderer renderer;
     /* Scene Editor */
     std::unique_ptr<SceneEditor> scene_editor;

public:
    Engine(const std::string& resources_root_hint = "", const std::string& scene_file_hint = "");
    ~Engine();
    void GameLoop();
};