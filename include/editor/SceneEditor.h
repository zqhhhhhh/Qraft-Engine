#pragma once

#include <memory>
#include <string>
#include <vector>
#include <array>
#include <unordered_set>
#include <unordered_map>
#include <optional>
#include <filesystem>
#include <utility>
#ifdef __APPLE__
#include <SDL.h>
#elif defined(__linux__)
#include <SDL2/SDL.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <SDL2/SDL.h>
#endif
#include "engine/Actor.h"
#include "engine/SceneDB.h"
#include "engine/TemplateDB.h"
#include "engine/ComponentDB.h"
#include "LuaBridge/LuaBridge.h"

class SceneEditor
{
public:
    SceneEditor() = default;
    ~SceneEditor();

    void Initialize(SceneDB* scene_db, TemplateDB* template_db, ComponentDB* component_db);
    void ProcessEvent(const SDL_Event& event);
    void RenderFrame(SDL_Window* window, SDL_Renderer* renderer);
    SDL_Rect GetGameViewportRect(SDL_Window* window) const;
    SDL_Rect GetEditorCenterAreaRect(SDL_Window* window) const;
    void CaptureGameFrame(SDL_Renderer* renderer, const SDL_Rect& viewport_rect);
    void RenderFrozenGameFrame(SDL_Renderer* renderer, const SDL_Rect& viewport_rect);
    void EnqueueEditorPreviewDrawRequests();
    bool HasPendingProjectLoadRequest() const { return pending_project_load_request; }
    std::string ConsumePendingProjectLoadResourcesPath();
    void NotifyProjectLoadResult(bool success, const std::string& message);
    void OnProjectLoaded();

    bool ShouldQuit() const { return quit_requested; }
    bool IsPlaying() const  { return is_playing; }
    bool IsShowingProjectHub() const { return show_project_hub; }

private:
    SceneDB* scene_db = nullptr;
    TemplateDB* template_db = nullptr;
    ComponentDB* component_db = nullptr;

    bool imgui_initialized = false;
    bool enabled = true;
    bool quit_requested = false;
    bool is_playing = false;

    std::shared_ptr<Actor> selected_entity = nullptr;
    std::shared_ptr<Actor> renaming_actor = nullptr;
    std::array<char, 128> rename_buf{};
    std::array<char, 64> new_entity_name{};
    std::array<char, 64> new_component_type{};
    std::array<char, 64> new_scene_name{};
    std::vector<std::string> scene_names;
    int selected_scene_index = -1;
    std::string saved_message;
    int saved_message_frames = 0;
    int last_window_width = 1280;
    int last_window_height = 720;

    static constexpr int kTopBarHeight = 100;
    static constexpr int kLeftPanelWidth = 320;
    static constexpr int kRightPanelWidth = 360;
    static constexpr int kResizeHandleHalfThickness = 5;

    bool BeginLayoutResize(int mouse_x, int mouse_y);
    void UpdateLayoutResize(int mouse_x, int mouse_y);
    void EndLayoutResize();
    bool IsResizingLayout() const;

    static bool TryGetNumber(const luabridge::LuaRef& table, const char* key, float& out_value);

    void InitializeImGui(SDL_Window* window, SDL_Renderer* renderer);
    void ShutdownImGui();
    void SyncInputModifiers();
    void RenderImGuiDrawData(SDL_Renderer* renderer);

    std::string CurrentSceneName() const;
    void RenderHierarchyPanel();
    void RenderInspectorPanel();
    void RenderControlsBar();
    void RenderProjectHub();
    void RefreshCustomComponentTypeList();
    void EnsureEngineLuaBuiltinScripts();
    void RenderBuiltinComponentProperties(luabridge::LuaRef& comp_ref, const std::string& type_name, const std::string& comp_key);
    void RenderComponentProperties(luabridge::LuaRef& comp_ref, const std::string& comp_key);
    void RefreshLuaScriptChanges();
    void RefreshSceneList();
    void SelectSceneInList(const std::string& scene_name);
    void HandleShortcutKeyDown(const SDL_KeyboardEvent& key_event);
    void HandleViewportSelectionClick(int mouse_x, int mouse_y);
    std::optional<std::pair<float, float>> TryGetActorWorldPosition(const std::shared_ptr<Actor>& actor) const;
    bool IsBuiltinComponentType(const std::string& type_name) const;
    bool EnterPlayMode();
    void ExitPlayMode();
    bool CreatePlayModeSceneBackup(const std::string& scene_name);
    void RestorePlayModeSceneBackup();

    void CreateEntity(const std::string& name);
    void DeleteEntity(std::shared_ptr<Actor> actor);
    void SaveScene(const std::string& scene_name);
    void ReloadScene(const std::string& scene_name);
    void AddComponentToActor(std::shared_ptr<Actor> actor, const std::string& type_name);
    void SyncActorTransformPosition(const std::shared_ptr<Actor>& actor);
    bool SaveActorAsTemplate(const std::shared_ptr<Actor>& actor, const std::string& template_name);
    void InstantiateTemplateInEditor(const std::string& template_name);

    int render_resolution_x = 640;
    int render_resolution_y = 360;
    float pixels_per_meter = 100.0f;
    int top_bar_height = kTopBarHeight;
    int left_panel_width = kLeftPanelWidth;
    int right_panel_width = kRightPanelWidth;
    bool resizing_top_bar = false;
    bool resizing_left_panel = false;
    bool resizing_right_panel = false;
    std::unordered_map<std::string, std::filesystem::file_time_type> lua_component_write_times;
    int lua_watch_frame_counter = 0;
    SDL_Texture* frozen_game_frame = nullptr;
    int frozen_game_frame_width = 0;
    int frozen_game_frame_height = 0;
    bool show_project_hub = true;
    bool pending_project_load_request = false;
    bool game_view_fullscreen = false;
    std::string pending_project_resources_path;
    std::string project_hub_message;
    std::string play_mode_scene_name;
    std::filesystem::path play_mode_scene_backup_path;
    std::array<char, 128> new_project_title{};
    std::array<char, 64> new_project_x_resolution{};
    std::array<char, 64> new_project_y_resolution{};
    std::string new_project_target_directory;
    std::vector<std::string> custom_component_types;
    int selected_builtin_component_index = 0;
    int selected_custom_component_index = 0;
    int selected_template_index = 0;
    std::array<char, 64> template_name_buf{};
    std::array<char, 64> instantiate_template_name_buf{};
};
