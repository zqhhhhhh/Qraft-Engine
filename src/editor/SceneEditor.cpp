#include "editor/SceneEditor.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <map>
#include <limits>
#include <cstdlib>
#include <cmath>

#include "box2d/extern/imgui/imgui.h"
#include "lua/lua.hpp"
#include "engine/utils/JsonUtils.h"
#include "renderer/Renderer.h"
#include "renderer/ImageDB.h"
#include "rapidjson/document.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/prettywriter.h"

namespace
{
    bool IsEngineLuaBuiltinComponent(const std::string& type_name)
    {
        return type_name == "SpriteRenderer"
            || type_name == "CameraManager"
            || type_name == "Transform"
            || type_name == "ConstantMovement"
            || type_name == "KeyboardControl"
            || type_name == "PlayAudio"
            || type_name == "TemplateSpawner";
    }

    std::string GetEngineLuaBuiltinScript(const std::string& type_name)
    {
        if (type_name == "SpriteRenderer")
        {
            return R"(SpriteRenderer = {
    sprite = "???",
    rotation = 0,
    scale_x = 1.0,
    scale_y = 1.0,
    pivot_x = 0.5,
    pivot_y = 0.5,
    r = 255,
    g = 255,
    b = 255,
    a = 255,
    sorting_order = 0,

    OnStart = function(self)
        self.rb = self.actor:GetComponent("Rigidbody")
        self.transform = self.actor:GetComponent("Transform")
    end,

    OnUpdate = function(self)
        if self.sprite == nil or self.sprite == "" or self.sprite == "???" then
            return
        end

        if self.rb == nil then
            self.rb = self.actor:GetComponent("Rigidbody")
        end
        if self.transform == nil then
            self.transform = self.actor:GetComponent("Transform")
        end

        local draw_x = self.x
        local draw_y = self.y
        local rot_degrees = self.rotation

        if self.rb ~= nil then
            local pos = self.rb:GetPosition()
            draw_x = pos.x
            draw_y = pos.y
            rot_degrees = self.rb:GetRotation()
        elseif self.transform ~= nil then
            draw_x = self.transform.x
            draw_y = self.transform.y
            self.x = draw_x
            self.y = draw_y
        end

        Image.DrawEx(self.sprite, draw_x, draw_y, rot_degrees, self.scale_x, self.scale_y, self.pivot_x, self.pivot_y, self.r, self.g, self.b, self.a, self.sorting_order)
    end
}
)";
        }
        if (type_name == "CameraManager")
        {
            return R"(CameraManager = {
    zoom_factor = 0.1,
    desired_zoom_factor = 1.0,
    x = 0.0,
    y = 0.0,
    ease_factor = 0.2,
    offset_y = -1,

    OnStart = function(self)
        Camera.SetZoom(self.zoom_factor)
    end,

    OnLateUpdate = function(self)
        local player = Actor.Find("Player")
        if player == nil then
            return
        end

        self.desired_zoom_factor = self.desired_zoom_factor + Input.GetMouseScrollDelta() * 0.1
        self.desired_zoom_factor = math.min(math.max(0.1, self.desired_zoom_factor), 2)
        self.zoom_factor = self.zoom_factor + (self.desired_zoom_factor - self.zoom_factor) * 0.1
        Camera.SetZoom(self.zoom_factor)

        local player_transform = player:GetComponent("Transform")
        local desired_x = player_transform.x
        local desired_y = player_transform.y + self.offset_y

        local mouse_pos = Input.GetMousePosition()
        desired_x = desired_x + (mouse_pos.x - 576) * 0.01
        desired_y = desired_y + (mouse_pos.y - 256) * 0.01

        self.x = self.x + (desired_x - self.x) * self.ease_factor
        self.y = self.y + (desired_y - self.y) * self.ease_factor
        Camera.SetPosition(self.x, self.y)
    end
}
)";
        }
        if (type_name == "Transform")
        {
            return R"(Transform = {
    x = 0,
    y = 0
}
)";
        }
        if (type_name == "ConstantMovement")
        {
            return R"(ConstantMovement = {
    x_vel = 0,
    y_vel = 0,

    OnStart = function(self)
        self.transform = self.actor:GetComponent("Transform")
    end,

    OnUpdate = function(self)
        if self.transform == nil then
            self.transform = self.actor:GetComponent("Transform")
            if self.transform == nil then
                return
            end
        end
        self.transform.x = self.transform.x + self.x_vel
        self.transform.y = self.transform.y + self.y_vel
    end
}
)";
        }
        if (type_name == "KeyboardControl")
        {
            return R"(KeyboardControl = {
    speed = 1,
    key_up = "w",
    key_down = "s",
    key_left = "a",
    key_right = "d",

    OnStart = function(self)
        self.transform = self.actor:GetComponent("Transform")
    end,

    OnUpdate = function(self)
        if self.transform == nil then
            self.transform = self.actor:GetComponent("Transform")
            if self.transform == nil then
                return
            end
        end

        local move_x = 0
        local move_y = 0
        if Input.GetKey(self.key_left)  then move_x = move_x - self.speed end
        if Input.GetKey(self.key_right) then move_x = move_x + self.speed end
        if Input.GetKey(self.key_up)    then move_y = move_y - self.speed end
        if Input.GetKey(self.key_down)  then move_y = move_y + self.speed end

        self.transform.x = self.transform.x + move_x
        self.transform.y = self.transform.y + move_y
    end
}
)";
        }
        if (type_name == "PlayAudio")
        {
            return R"(PlayAudio = {
    clip = "",
    channel = 0,
    loop = false,
    play_on_start = true,
    stop_on_destroy = false,

    Play = function(self)
        if self.clip ~= nil and self.clip ~= "" then
            Audio.Play(self.channel, self.clip, self.loop)
        end
    end,

    Stop = function(self)
        Audio.Halt(self.channel)
    end,

    OnStart = function(self)
        if self.play_on_start then
            self:Play()
        end
    end,

    OnDestroy = function(self)
        if self.stop_on_destroy then
            self:Stop()
        end
    end
}
)";
        }
        if (type_name == "TemplateSpawner")
        {
            return R"(TemplateSpawner = {
    template_name = "???",
    frame_delay = 1,
    spawn_x = 0,
    spawn_y = 0,

    OnUpdate = function(self)
        if game_state == "gameover" then
            return
        end

        local delay = self.frame_delay
        if delay == nil or delay <= 0 then
            delay = 1
        end

        if Application.GetFrame() % delay == 0 then
            local new_actor = Actor.Instantiate(self.template_name)
            if new_actor ~= nil then
                local new_actor_t = new_actor:GetComponent("Transform")
                if new_actor_t ~= nil then
                    new_actor_t.x = self.spawn_x
                    new_actor_t.y = self.spawn_y
                end
            end
        end
    end
}
)";
        }
        return "";
    }

    std::string EscapeJsonString(const std::string& input)
    {
        std::string out;
        out.reserve(input.size() + 8);
        for (char c : input)
        {
            switch (c)
            {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
            }
        }
        return out;
    }

    std::string TrimString(const std::string& str)
    {
        size_t start = 0;
        while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start])))
        {
            ++start;
        }
        size_t end = str.size();
        while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1])))
        {
            --end;
        }
        return str.substr(start, end - start);
    }

    std::string NormalizeComponentTypeName(const std::string& raw)
    {
        std::string name = TrimString(raw);
        if (name.size() >= 4 && name.substr(name.size() - 4) == ".lua")
        {
            name = name.substr(0, name.size() - 4);
        }
        return TrimString(name);
    }

    bool IsValidLuaIdentifier(const std::string& name)
    {
        if (name.empty())
        {
            return false;
        }
        const unsigned char first = static_cast<unsigned char>(name.front());
        if (!(std::isalpha(first) || first == '_'))
        {
            return false;
        }
        for (const char ch : name)
        {
            const unsigned char uch = static_cast<unsigned char>(ch);
            if (!(std::isalnum(uch) || uch == '_'))
            {
                return false;
            }
        }
        return true;
    }

    std::string BuildComponentTemplate(const std::string& type_name)
    {
        return type_name + " = {\n}\n";
    }

    std::shared_ptr<luabridge::LuaRef> FindComponentByType(const std::shared_ptr<Actor>& actor, const std::string& type_name)
    {
        if (!actor)
        {
            return nullptr;
        }
        for (const auto& [key, comp_ptr] : actor->components)
        {
            (void)key;
            if (!comp_ptr)
            {
                continue;
            }
            luabridge::LuaRef type_ref = (*comp_ptr)["type"];
            if (type_ref.isString() && type_ref.cast<std::string>() == type_name)
            {
                return comp_ptr;
            }
        }
        return nullptr;
    }

    bool TryReadComponentXY(const luabridge::LuaRef& component_ref, float& out_x, float& out_y)
    {
        luabridge::LuaRef x_ref = component_ref["x"];
        luabridge::LuaRef y_ref = component_ref["y"];
        if (!x_ref.isNumber() || !y_ref.isNumber())
        {
            return false;
        }
        out_x = x_ref.cast<float>();
        out_y = y_ref.cast<float>();
        return true;
    }

    void WriteComponentXY(luabridge::LuaRef& component_ref, float x, float y)
    {
        component_ref["x"] = x;
        component_ref["y"] = y;
    }

    std::optional<std::pair<float, float>> GetPreferredActorPosition(const std::shared_ptr<Actor>& actor)
    {
        if (!actor)
        {
            return std::nullopt;
        }

        if (std::shared_ptr<luabridge::LuaRef> rb = FindComponentByType(actor, "Rigidbody"))
        {
            float x = 0.0f;
            float y = 0.0f;
            if (TryReadComponentXY(*rb, x, y))
            {
                return std::make_pair(x, y);
            }
        }
        if (std::shared_ptr<luabridge::LuaRef> sr = FindComponentByType(actor, "SpriteRenderer"))
        {
            float x = 0.0f;
            float y = 0.0f;
            if (TryReadComponentXY(*sr, x, y))
            {
                return std::make_pair(x, y);
            }
        }
        return std::nullopt;
    }

    std::optional<std::string> ImportImageFileToProject(const std::filesystem::path& src_path)
    {
        if (src_path.empty() || !std::filesystem::exists(src_path) || !src_path.has_stem())
        {
            return std::nullopt;
        }

        const std::string ext = src_path.has_extension() ? src_path.extension().string() : "";
        std::string lower_ext = ext;
        std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (lower_ext.empty())
        {
            return std::nullopt;
        }

        std::filesystem::create_directories("resources/images");
        const std::filesystem::path dst = std::filesystem::path("resources/images") / (src_path.stem().string() + ".png");

        std::error_code ec;
        const std::filesystem::path abs_src = std::filesystem::weakly_canonical(src_path, ec);
        ec.clear();
        const std::filesystem::path abs_dst = std::filesystem::weakly_canonical(dst, ec);
        if (!abs_src.empty() && !abs_dst.empty() && abs_src == abs_dst)
        {
            return src_path.stem().string();
        }

        if (lower_ext == ".png")
        {
            std::filesystem::copy_file(src_path, dst, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec)
            {
                return std::nullopt;
            }
        }
        else
        {
            std::string cmd = "sips -s format png \"" + std::filesystem::absolute(src_path).string() + "\" --out \"" + std::filesystem::absolute(dst).string() + "\" >/dev/null";
            if (std::system(cmd.c_str()) != 0)
            {
                return std::nullopt;
            }
        }

        return src_path.stem().string();
    }

    std::optional<std::string> PickImageAndImportToProject()
    {
#ifdef __APPLE__
        const char* pick_cmd =
            "osascript -e 'set f to choose file with prompt \"Pick an image\" of type {\"public.image\"}' -e 'POSIX path of f'";
        FILE* pipe = popen(pick_cmd, "r");
        if (pipe == nullptr)
        {
            return std::nullopt;
        }

        std::string selected_path;
        char buffer[1024];
        while (fgets(buffer, static_cast<int>(sizeof(buffer)), pipe) != nullptr)
        {
            selected_path += buffer;
        }
        const int status = pclose(pipe);
        if (status != 0)
        {
            return std::nullopt;
        }

        selected_path = TrimString(selected_path);
        if (selected_path.empty())
        {
            return std::nullopt;
        }

        return ImportImageFileToProject(std::filesystem::path(selected_path));
#else
        return std::nullopt;
#endif
    }

    // Copy/convert an audio file into resources/audio/ and return the stem name used by Audio.Play().
    // Supported engine formats: .wav and .ogg.  Other formats (e.g. .mp3) are converted to .wav via afconvert.
    std::optional<std::string> ImportAudioFileToProject(const std::filesystem::path& src_path)
    {
        if (src_path.empty() || !std::filesystem::exists(src_path) || !src_path.has_stem())
            return std::nullopt;

        const std::string ext = src_path.has_extension() ? src_path.extension().string() : "";
        std::string lower_ext = ext;
        std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        std::filesystem::create_directories("resources/audio");

        std::error_code ec;

        if (lower_ext == ".wav" || lower_ext == ".ogg")
        {
            // Copy directly — engine already supports these formats
            const std::filesystem::path dst =
                std::filesystem::path("resources/audio") / (src_path.stem().string() + lower_ext);

            const std::filesystem::path abs_src = std::filesystem::weakly_canonical(src_path, ec);
            ec.clear();
            const std::filesystem::path abs_dst = std::filesystem::weakly_canonical(dst, ec);
            if (!abs_src.empty() && !abs_dst.empty() && abs_src == abs_dst)
                return src_path.stem().string();

            std::filesystem::copy_file(src_path, dst, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) return std::nullopt;
            return src_path.stem().string();
        }
        else
        {
            // Convert to .wav using macOS afconvert (handles mp3, aiff, m4a, aac, …)
            const std::filesystem::path dst =
                std::filesystem::path("resources/audio") / (src_path.stem().string() + ".wav");
            std::string cmd = "afconvert -f WAVE -d LEI16 \""
                + std::filesystem::absolute(src_path).string() + "\" \""
                + std::filesystem::absolute(dst).string() + "\" 2>/dev/null";
            if (std::system(cmd.c_str()) == 0)
                return src_path.stem().string();

            // afconvert failed — fall back to a plain copy so the file at least lands in the project
            std::filesystem::copy_file(src_path, dst, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) return std::nullopt;
            return src_path.stem().string();
        }
    }

    // Open a system file-picker filtered to audio files and import the result.
    std::optional<std::string> PickAudioAndImportToProject()
    {
#ifdef __APPLE__
        const char* pick_cmd =
            "osascript -e 'set f to choose file with prompt \"Pick an audio file\" "
            "of type {\"public.audio\", \"public.mp3\", \"org.xiph.ogg-vorbis\"}' "
            "-e 'POSIX path of f'";
        FILE* pipe = popen(pick_cmd, "r");
        if (pipe == nullptr) return std::nullopt;

        std::string selected_path;
        char buffer[1024];
        while (fgets(buffer, static_cast<int>(sizeof(buffer)), pipe) != nullptr)
            selected_path += buffer;

        const int status = pclose(pipe);
        if (status != 0) return std::nullopt;

        selected_path = TrimString(selected_path);
        if (selected_path.empty()) return std::nullopt;

        return ImportAudioFileToProject(std::filesystem::path(selected_path));
#else
        return std::nullopt;
#endif
    }

    std::optional<std::string> ChooseDirectoryDialog()
    {
#ifdef __APPLE__
        const char* pick_cmd = "osascript -e 'set d to choose folder with prompt \"Choose folder\"' -e 'POSIX path of d'";
        FILE* pipe = popen(pick_cmd, "r");
        if (pipe == nullptr)
        {
            return std::nullopt;
        }

        std::string selected_path;
        char buffer[1024];
        while (fgets(buffer, static_cast<int>(sizeof(buffer)), pipe) != nullptr)
        {
            selected_path += buffer;
        }
        const int status = pclose(pipe);
        if (status != 0)
        {
            return std::nullopt;
        }

        selected_path = TrimString(selected_path);
        if (selected_path.empty())
        {
            return std::nullopt;
        }
        return selected_path;
#else
        return std::nullopt;
#endif
    }

    bool CopyFileIfExists(const std::filesystem::path& src, const std::filesystem::path& dst)
    {
        if (!std::filesystem::exists(src))
        {
            return false;
        }
        std::error_code ec;
        std::filesystem::create_directories(dst.parent_path(), ec);
        std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
        return !ec;
    }

    bool IsResourcesFolderPath(std::filesystem::path path)
    {
        path = path.lexically_normal();
        if (path.filename().empty())
        {
            path = path.parent_path();
        }
        return path.filename() == "resources";
    }

    const char* ImGuiGetClipboardText(void*)
    {
        static std::string clipboard_cache;
        char* text = SDL_GetClipboardText();
        if (text == nullptr)
        {
            clipboard_cache.clear();
            return clipboard_cache.c_str();
        }
        clipboard_cache = text;
        SDL_free(text);
        return clipboard_cache.c_str();
    }

    void ImGuiSetClipboardText(void*, const char* text)
    {
        if (text == nullptr)
        {
            return;
        }
        SDL_SetClipboardText(text);
    }
}

SceneEditor::~SceneEditor()
{
    if (frozen_game_frame != nullptr)
    {
        SDL_DestroyTexture(frozen_game_frame);
        frozen_game_frame = nullptr;
    }
    ShutdownImGui();
}

void SceneEditor::Initialize(SceneDB* scene_db, TemplateDB* template_db, ComponentDB* component_db)
{
    // Ensure all necessary resource directories exist
    std::filesystem::create_directories("resources/scenes");
    std::filesystem::create_directories("resources/component_types");
    std::filesystem::create_directories("resources/actor_templates");

    this->scene_db = scene_db;
    this->template_db = template_db;
    this->component_db = component_db;
    std::snprintf(new_entity_name.data(), new_entity_name.size(), "NewActor");
    std::snprintf(new_scene_name.data(), new_scene_name.size(), "scene1");
    std::snprintf(template_name_buf.data(), template_name_buf.size(), "");
    std::snprintf(instantiate_template_name_buf.data(), instantiate_template_name_buf.size(), "");
    std::snprintf(new_project_title.data(), new_project_title.size(), "New Project");
    std::snprintf(new_project_x_resolution.data(), new_project_x_resolution.size(), "640");
    std::snprintf(new_project_y_resolution.data(), new_project_y_resolution.size(), "360");
    new_project_target_directory = std::filesystem::current_path().string();

    EnsureEngineLuaBuiltinScripts();
    RefreshCustomComponentTypeList();

    if (std::filesystem::exists("resources/rendering.config"))
    {
        rapidjson::Document rendering_config;
        JsonUtils::ReadJsonFile("resources/rendering.config", rendering_config);
        if (rendering_config.HasMember("x_resolution") && rendering_config["x_resolution"].IsInt())
        {
            render_resolution_x = std::max(1, rendering_config["x_resolution"].GetInt());
        }
        if (rendering_config.HasMember("y_resolution") && rendering_config["y_resolution"].IsInt())
        {
            render_resolution_y = std::max(1, rendering_config["y_resolution"].GetInt());
        }
    }

    RefreshSceneList();
    SelectSceneInList(CurrentSceneName());
}

void SceneEditor::EnsureEngineLuaBuiltinScripts()
{
    std::filesystem::create_directories("resources/component_types");
    static const std::vector<std::string> kBuiltinLuaTypes = {
        "SpriteRenderer", "CameraManager", "Transform",
        "ConstantMovement", "KeyboardControl", "PlayAudio", "TemplateSpawner"
    };
    for (const std::string& type_name : kBuiltinLuaTypes)
    {
        const std::filesystem::path script_path = std::filesystem::path("resources/component_types") / (type_name + ".lua");
        if (std::filesystem::exists(script_path))
        {
            continue;
        }
        std::ofstream out(script_path);
        if (!out.is_open())
        {
            continue;
        }
        out << GetEngineLuaBuiltinScript(type_name);
    }
}

void SceneEditor::RefreshCustomComponentTypeList()
{
    custom_component_types.clear();
    const std::filesystem::path script_dir("resources/component_types");
    if (std::filesystem::exists(script_dir))
    {
        for (const auto& entry : std::filesystem::directory_iterator(script_dir))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".lua")
            {
                continue;
            }
            const std::string type_name = entry.path().stem().string();
            if (!IsEngineLuaBuiltinComponent(type_name) && !IsBuiltinComponentType(type_name))
            {
                custom_component_types.push_back(type_name);
            }
        }
    }
    std::sort(custom_component_types.begin(), custom_component_types.end());
    custom_component_types.erase(std::unique(custom_component_types.begin(), custom_component_types.end()), custom_component_types.end());
    if (selected_custom_component_index >= static_cast<int>(custom_component_types.size()))
    {
        selected_custom_component_index = 0;
    }
}

void SceneEditor::EnqueueEditorPreviewDrawRequests()
{
    if (is_playing || show_project_hub || scene_db == nullptr)
    {
        return;
    }

    for (const auto& actor : scene_db->GetActors())
    {
        if (!actor)
        {
            continue;
        }

        float rb_x = 0.0f;
        float rb_y = 0.0f;
        float rb_rot = 0.0f;
        bool has_rb = false;

        for (const auto& [component_key, component_ref_ptr] : actor->components)
        {
            (void)component_key;
            if (!component_ref_ptr)
            {
                continue;
            }
            luabridge::LuaRef comp = *component_ref_ptr;
            luabridge::LuaRef type_ref = comp["type"];
            if (!type_ref.isString() || type_ref.cast<std::string>() != "Rigidbody")
            {
                continue;
            }
            luabridge::LuaRef enabled_ref = comp["enabled"];
            if (enabled_ref.isBool() && !enabled_ref.cast<bool>())
            {
                continue;
            }
            luabridge::LuaRef x_ref = comp["x"];
            luabridge::LuaRef y_ref = comp["y"];
            luabridge::LuaRef rot_ref = comp["rotation"];
            if (x_ref.isNumber() && y_ref.isNumber())
            {
                rb_x = x_ref.cast<float>();
                rb_y = y_ref.cast<float>();
                rb_rot = rot_ref.isNumber() ? rot_ref.cast<float>() : 0.0f;
                has_rb = true;
            }
            break;
        }

        for (const auto& [component_key, component_ref_ptr] : actor->components)
        {
            (void)component_key;
            if (!component_ref_ptr)
            {
                continue;
            }
            luabridge::LuaRef comp = *component_ref_ptr;
            if (!comp.isTable())
            {
                continue;
            }
            luabridge::LuaRef type_ref = comp["type"];
            if (!type_ref.isString() || type_ref.cast<std::string>() != "SpriteRenderer")
            {
                continue;
            }
            luabridge::LuaRef enabled_ref = comp["enabled"];
            if (enabled_ref.isBool() && !enabled_ref.cast<bool>())
            {
                continue;
            }

            luabridge::LuaRef sprite_ref = comp["sprite"];
            if (!sprite_ref.isString())
            {
                continue;
            }
            const std::string sprite_name = sprite_ref.cast<std::string>();
            if (sprite_name.empty() || sprite_name == "???")
            {
                continue;
            }

            float x = has_rb ? rb_x : (comp["x"].isNumber() ? comp["x"].cast<float>() : 0.0f);
            float y = has_rb ? rb_y : (comp["y"].isNumber() ? comp["y"].cast<float>() : 0.0f);
            float rotation = has_rb ? rb_rot : (comp["rotation"].isNumber() ? comp["rotation"].cast<float>() : 0.0f);
            float scale_x = comp["scale_x"].isNumber() ? comp["scale_x"].cast<float>() : 1.0f;
            float scale_y = comp["scale_y"].isNumber() ? comp["scale_y"].cast<float>() : 1.0f;
            float pivot_x = comp["pivot_x"].isNumber() ? comp["pivot_x"].cast<float>() : 0.5f;
            float pivot_y = comp["pivot_y"].isNumber() ? comp["pivot_y"].cast<float>() : 0.5f;
            float r = comp["r"].isNumber() ? comp["r"].cast<float>() : 255.0f;
            float g = comp["g"].isNumber() ? comp["g"].cast<float>() : 255.0f;
            float b = comp["b"].isNumber() ? comp["b"].cast<float>() : 255.0f;
            float a = comp["a"].isNumber() ? comp["a"].cast<float>() : 255.0f;
            float sorting = comp["sorting_order"].isNumber() ? comp["sorting_order"].cast<float>() : 0.0f;

            ImageDB::LuaDrawEx(sprite_name, x, y, rotation, scale_x, scale_y, pivot_x, pivot_y, r, g, b, a, sorting);
        }
    }
}

void SceneEditor::InitializeImGui(SDL_Window* window, SDL_Renderer* renderer)
{
    if (imgui_initialized || window == nullptr || renderer == nullptr)
    {
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.BackendPlatformName = "custom_sdl";
    io.BackendRendererName = "custom_sdl_renderer";
    io.IniFilename = nullptr;
    io.GetClipboardTextFn = ImGuiGetClipboardText;
    io.SetClipboardTextFn = ImGuiSetClipboardText;
    io.ClipboardUserData = nullptr;
    io.KeyMap[ImGuiKey_Tab] = SDL_SCANCODE_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
    io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
    io.KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
    io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
    io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
    io.KeyMap[ImGuiKey_Insert] = SDL_SCANCODE_INSERT;
    io.KeyMap[ImGuiKey_Delete] = SDL_SCANCODE_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = SDL_SCANCODE_BACKSPACE;
    io.KeyMap[ImGuiKey_Space] = SDL_SCANCODE_SPACE;
    io.KeyMap[ImGuiKey_Enter] = SDL_SCANCODE_RETURN;
    io.KeyMap[ImGuiKey_Escape] = SDL_SCANCODE_ESCAPE;
    io.KeyMap[ImGuiKey_A] = SDL_SCANCODE_A;
    io.KeyMap[ImGuiKey_C] = SDL_SCANCODE_C;
    io.KeyMap[ImGuiKey_V] = SDL_SCANCODE_V;
    io.KeyMap[ImGuiKey_X] = SDL_SCANCODE_X;
    io.KeyMap[ImGuiKey_Y] = SDL_SCANCODE_Y;
    io.KeyMap[ImGuiKey_Z] = SDL_SCANCODE_Z;

    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    SDL_Texture* font_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, width, height);
    if (font_texture != nullptr)
    {
        SDL_SetTextureBlendMode(font_texture, SDL_BLENDMODE_BLEND);
        SDL_UpdateTexture(font_texture, nullptr, pixels, width * 4);
        io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(font_texture));
    }

    imgui_initialized = true;
}

void SceneEditor::ShutdownImGui()
{
    if (!imgui_initialized)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    SDL_Texture* font_texture = reinterpret_cast<SDL_Texture*>(io.Fonts->TexID);
    if (font_texture != nullptr)
    {
        SDL_DestroyTexture(font_texture);
        io.Fonts->SetTexID(nullptr);
    }

    ImGui::DestroyContext();
    imgui_initialized = false;
}

void SceneEditor::SyncInputModifiers()
{
    ImGuiIO& io = ImGui::GetIO();
    SDL_Keymod key_mod = SDL_GetModState();
#ifdef __APPLE__
    io.KeyCtrl = (key_mod & KMOD_CTRL) != 0 || (key_mod & KMOD_GUI) != 0;
#else
    io.KeyCtrl = (key_mod & KMOD_CTRL) != 0;
#endif
    io.KeyShift = (key_mod & KMOD_SHIFT) != 0;
    io.KeyAlt = (key_mod & KMOD_ALT) != 0;
    io.KeySuper = (key_mod & KMOD_GUI) != 0;
}

bool SceneEditor::IsResizingLayout() const
{
    return resizing_top_bar || resizing_left_panel || resizing_right_panel;
}

bool SceneEditor::BeginLayoutResize(int mouse_x, int mouse_y)
{
    if (show_project_hub)
    {
        return false;
    }

    const bool near_top_bar = std::abs(mouse_y - top_bar_height) <= kResizeHandleHalfThickness;
    if (near_top_bar)
    {
        resizing_top_bar = true;
        return true;
    }

    if (game_view_fullscreen || mouse_y < top_bar_height)
    {
        return false;
    }

    const bool near_left = std::abs(mouse_x - left_panel_width) <= kResizeHandleHalfThickness;
    const int right_split_x = std::max(0, last_window_width - right_panel_width);
    const bool near_right = std::abs(mouse_x - right_split_x) <= kResizeHandleHalfThickness;

    if (near_left)
    {
        resizing_left_panel = true;
        return true;
    }
    if (near_right)
    {
        resizing_right_panel = true;
        return true;
    }

    return false;
}

void SceneEditor::UpdateLayoutResize(int mouse_x, int mouse_y)
{
    const int min_center_width = 200;

    if (resizing_top_bar)
    {
        const int max_top = std::max(64, last_window_height - 120);
        top_bar_height = std::clamp(mouse_y, 64, max_top);
    }

    if (resizing_left_panel)
    {
        const int max_left = std::max(180, last_window_width - right_panel_width - min_center_width);
        left_panel_width = std::clamp(mouse_x, 180, max_left);
    }

    if (resizing_right_panel)
    {
        const int min_split_x = left_panel_width + min_center_width;
        const int split_x = std::clamp(mouse_x, min_split_x, std::max(min_split_x, last_window_width - 180));
        right_panel_width = std::clamp(last_window_width - split_x, 180, std::max(180, last_window_width - left_panel_width - min_center_width));
    }
}

void SceneEditor::EndLayoutResize()
{
    resizing_top_bar = false;
    resizing_left_panel = false;
    resizing_right_panel = false;
}

void SceneEditor::ProcessEvent(const SDL_Event& event)
{
    if (!imgui_initialized)
    {
        return;
    }

    if (event.type == SDL_KEYDOWN)
    {
        HandleShortcutKeyDown(event.key);
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && enabled)
    {
        if (BeginLayoutResize(event.button.x, event.button.y))
        {
            return;
        }
    }

    if (event.type == SDL_MOUSEMOTION && IsResizingLayout())
    {
        UpdateLayoutResize(event.motion.x, event.motion.y);
        return;
    }

    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
    {
        EndLayoutResize();
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && enabled && !IsResizingLayout())
    {
        HandleViewportSelectionClick(event.button.x, event.button.y);
    }

    if (!enabled)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    switch (event.type)
    {
    case SDL_MOUSEWHEEL:
        io.MouseWheel += static_cast<float>(event.wheel.y);
        io.MouseWheelH += static_cast<float>(event.wheel.x);
        break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        if (event.button.button == SDL_BUTTON_LEFT) io.MouseDown[0] = (event.type == SDL_MOUSEBUTTONDOWN);
        if (event.button.button == SDL_BUTTON_RIGHT) io.MouseDown[1] = (event.type == SDL_MOUSEBUTTONDOWN);
        if (event.button.button == SDL_BUTTON_MIDDLE) io.MouseDown[2] = (event.type == SDL_MOUSEBUTTONDOWN);
        break;
    case SDL_TEXTINPUT:
        io.AddInputCharactersUTF8(event.text.text);
        break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        if (event.key.keysym.scancode < SDL_NUM_SCANCODES)
        {
            io.KeysDown[event.key.keysym.scancode] = (event.type == SDL_KEYDOWN);
        }
        SyncInputModifiers();
        break;
    default:
        break;
    }
}

void SceneEditor::RenderFrame(SDL_Window* window, SDL_Renderer* renderer)
{
    if (!enabled || !scene_db || window == nullptr || renderer == nullptr)
    {
        return;
    }

    InitializeImGui(window, renderer);

    ImGuiIO& io = ImGui::GetIO();
    int window_width = 0;
    int window_height = 0;
    SDL_GetWindowSize(window, &window_width, &window_height);
    last_window_width = std::max(window_width, 1);
    last_window_height = std::max(window_height, 1);
    io.DisplaySize = ImVec2(static_cast<float>(window_width), static_cast<float>(window_height));
    io.DeltaTime = (io.DeltaTime > 0.0f) ? io.DeltaTime : (1.0f / 60.0f);

    int mouse_x = 0;
    int mouse_y = 0;
    const Uint32 mouse_state = SDL_GetMouseState(&mouse_x, &mouse_y);
    io.MousePos = ImVec2(static_cast<float>(mouse_x), static_cast<float>(mouse_y));
    io.MouseDown[0] = (mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    io.MouseDown[1] = (mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
    io.MouseDown[2] = (mouse_state & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;

    RefreshLuaScriptChanges();

    ImGui::NewFrame();

    if (show_project_hub)
    {
        RenderProjectHub();
    }
    else
    {
        RenderControlsBar();
        if (!game_view_fullscreen)
        {
            RenderHierarchyPanel();
            RenderInspectorPanel();
        }
    }

    ImGui::Render();
    RenderImGuiDrawData(renderer);

    if (saved_message_frames > 0)
    {
        --saved_message_frames;
    }
    else
    {
        saved_message.clear();
    }
}

std::string SceneEditor::ConsumePendingProjectLoadResourcesPath()
{
    pending_project_load_request = false;
    std::string path;
    path.swap(pending_project_resources_path);
    return path;
}

void SceneEditor::NotifyProjectLoadResult(bool success, const std::string& message)
{
    project_hub_message = message;
    if (success)
    {
        show_project_hub = false;
    }
    else
    {
        show_project_hub = true;
    }
}

void SceneEditor::OnProjectLoaded()
{
    if (std::filesystem::exists("resources/rendering.config"))
    {
        rapidjson::Document rendering_config;
        JsonUtils::ReadJsonFile("resources/rendering.config", rendering_config);
        if (rendering_config.HasMember("x_resolution") && rendering_config["x_resolution"].IsInt())
        {
            render_resolution_x = std::max(1, rendering_config["x_resolution"].GetInt());
        }
        if (rendering_config.HasMember("y_resolution") && rendering_config["y_resolution"].IsInt())
        {
            render_resolution_y = std::max(1, rendering_config["y_resolution"].GetInt());
        }
    }

    EnsureEngineLuaBuiltinScripts();
    RefreshCustomComponentTypeList();
    selected_entity.reset();
    RefreshSceneList();
    SelectSceneInList(CurrentSceneName());
    lua_component_write_times.clear();
    saved_message = "Project loaded";
    saved_message_frames = 180;
}

void SceneEditor::RenderProjectHub()
{
    const float hub_width = std::min(760.0f, static_cast<float>(last_window_width) - 40.0f);
    const float hub_height = std::min(560.0f, static_cast<float>(last_window_height) - 40.0f);
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(last_window_width) * 0.5f, static_cast<float>(last_window_height) * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::max(360.0f, hub_width), std::max(360.0f, hub_height)), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::Begin("Project Hub", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    ImGui::PopStyleColor();

    ImGui::SetCursorPosX(std::max(0.0f, ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - 28.0f));
    ImGui::SetCursorPosY(std::max(0.0f, ImGui::GetStyle().WindowPadding.y));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.10f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.05f, 0.05f, 1.0f));
    if (ImGui::Button("X", ImVec2(24.0f, 24.0f)))
    {
        quit_requested = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::Separator();

    ImGui::TextUnformatted("New Project");
    ImGui::PushItemWidth(280.0f);
    ImGui::InputText("Game Title", new_project_title.data(), new_project_title.size());
    ImGui::InputText("X Resolution", new_project_x_resolution.data(), new_project_x_resolution.size());
    ImGui::InputText("Y Resolution", new_project_y_resolution.data(), new_project_y_resolution.size());
    ImGui::PopItemWidth();

    ImGui::TextWrapped("Target Folder: %s", new_project_target_directory.c_str());
    if (ImGui::Button("Choose Location"))
    {
        std::optional<std::string> selected = ChooseDirectoryDialog();
        if (selected.has_value())
        {
            new_project_target_directory = selected.value();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Create Project"))
    {
        try
        {
            const std::string title = TrimString(new_project_title.data());
            const int x_res = std::stoi(TrimString(new_project_x_resolution.data()));
            const int y_res = std::stoi(TrimString(new_project_y_resolution.data()));

            if (title.empty() || x_res <= 0 || y_res <= 0 || new_project_target_directory.empty())
            {
                project_hub_message = "Invalid project fields";
            }
            else
            {
                const std::filesystem::path resources_root = std::filesystem::path(new_project_target_directory) / "resources";
                if (std::filesystem::exists(resources_root))
                {
                    project_hub_message = "Target already contains a resources folder";
                }
                else
                {
                    std::filesystem::create_directories(resources_root / "scenes");
                    std::filesystem::create_directories(resources_root / "component_types");
                    std::filesystem::create_directories(resources_root / "images");
                    std::filesystem::create_directories(resources_root / "actor_templates");

                    {
                        std::ofstream cfg(resources_root / "game.config");
                        cfg << "{\"game_title\": \"" << EscapeJsonString(title) << "\", \"initial_scene\": \"scene0\"}";
                    }
                    {
                        std::ofstream rcfg(resources_root / "rendering.config");
                        rcfg << "{\n"
                             << "  \"x_resolution\": " << x_res << ",\n"
                             << "  \"y_resolution\": " << y_res << ",\n"
                             << "  \"clear_color_r\": 0,\n"
                             << "  \"clear_color_g\": 0,\n"
                             << "  \"clear_color_b\": 0,\n"
                             << "  \"cam_offset_x\": 0.0,\n"
                             << "  \"cam_offset_y\": 0.0\n"
                             << "}\n";
                    }
                    {
                        std::ofstream scene(resources_root / "scenes/scene0.scene");
                        scene << "{\"actors\": []}";
                    }

                    const std::filesystem::path src_sprite = std::filesystem::path("resources/component_types/SpriteRenderer.lua");
                    const std::filesystem::path src_camera = std::filesystem::path("resources/component_types/CameraManager.lua");
                    const std::filesystem::path dst_sprite = resources_root / "component_types/SpriteRenderer.lua";
                    const std::filesystem::path dst_camera = resources_root / "component_types/CameraManager.lua";

                    if (!CopyFileIfExists(src_sprite, dst_sprite))
                    {
                        std::ofstream fallback(dst_sprite);
                        fallback << BuildComponentTemplate("SpriteRenderer");
                    }
                    if (!CopyFileIfExists(src_camera, dst_camera))
                    {
                        std::ofstream fallback(dst_camera);
                        fallback << BuildComponentTemplate("CameraManager");
                    }

                    pending_project_resources_path = resources_root.string();
                    pending_project_load_request = true;
                    project_hub_message = "Loading new project...";
                }
            }
        }
        catch (...)
        {
            project_hub_message = "Invalid resolution input";
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Open Existing Project");
    if (ImGui::Button("Choose resources folder"))
    {
        std::optional<std::string> selected = ChooseDirectoryDialog();
        if (!selected.has_value())
        {
            project_hub_message = "No folder selected";
        }
        else
        {
            std::filesystem::path p(selected.value());
            p = p.lexically_normal();
            if (p.filename().empty())
            {
                p = p.parent_path();
            }

            if (!IsResourcesFolderPath(p))
            {
                project_hub_message = "Selected folder must be named resources";
            }
            else if (!std::filesystem::exists(p / "game.config"))
            {
                project_hub_message = "Invalid resources folder: game.config missing";
            }
            else
            {
                pending_project_resources_path = p.string();
                pending_project_load_request = true;
                project_hub_message = "Loading existing project...";
            }
        }
    }

    if (!project_hub_message.empty())
    {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", project_hub_message.c_str());
    }

    ImGui::End();
}

void SceneEditor::CaptureGameFrame(SDL_Renderer* renderer, const SDL_Rect& viewport_rect)
{
    if (renderer == nullptr || viewport_rect.w <= 0 || viewport_rect.h <= 0)
    {
        return;
    }

    const int width = viewport_rect.w;
    const int height = viewport_rect.h;
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    if (SDL_RenderReadPixels(renderer, &viewport_rect, SDL_PIXELFORMAT_ABGR8888, pixels.data(), width * 4) != 0)
    {
        return;
    }

    if (frozen_game_frame == nullptr || frozen_game_frame_width != width || frozen_game_frame_height != height)
    {
        if (frozen_game_frame != nullptr)
        {
            SDL_DestroyTexture(frozen_game_frame);
            frozen_game_frame = nullptr;
        }

        frozen_game_frame = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, width, height);
        if (frozen_game_frame == nullptr)
        {
            frozen_game_frame_width = 0;
            frozen_game_frame_height = 0;
            return;
        }
        SDL_SetTextureBlendMode(frozen_game_frame, SDL_BLENDMODE_NONE);
        frozen_game_frame_width = width;
        frozen_game_frame_height = height;
    }

    SDL_UpdateTexture(frozen_game_frame, nullptr, pixels.data(), width * 4);
}

void SceneEditor::RenderFrozenGameFrame(SDL_Renderer* renderer, const SDL_Rect& viewport_rect)
{
    if (renderer == nullptr || frozen_game_frame == nullptr || viewport_rect.w <= 0 || viewport_rect.h <= 0)
    {
        return;
    }

    SDL_RenderCopy(renderer, frozen_game_frame, nullptr, &viewport_rect);
}

void SceneEditor::RefreshLuaScriptChanges()
{
    if (is_playing || component_db == nullptr || scene_db == nullptr)
    {
        return;
    }

    ++lua_watch_frame_counter;
    if (lua_watch_frame_counter % 10 != 0)
    {
        return;
    }

    const std::filesystem::path script_dir("resources/component_types");
    if (!std::filesystem::exists(script_dir))
    {
        lua_component_write_times.clear();
        return;
    }

    bool changed = false;
    std::unordered_set<std::string> seen;

    for (const auto& entry : std::filesystem::directory_iterator(script_dir))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".lua")
        {
            continue;
        }

        const std::string type_name = entry.path().stem().string();
        seen.insert(type_name);

        const auto write_time = std::filesystem::last_write_time(entry.path());
        auto it = lua_component_write_times.find(type_name);
        if (it == lua_component_write_times.end())
        {
            lua_component_write_times[type_name] = write_time;
            continue;
        }

        if (it->second != write_time)
        {
            it->second = write_time;
            changed = true;
        }
    }

    for (auto it = lua_component_write_times.begin(); it != lua_component_write_times.end(); )
    {
        if (seen.find(it->first) == seen.end())
        {
            it = lua_component_write_times.erase(it);
            changed = true;
        }
        else
        {
            ++it;
        }
    }

    if (!changed)
    {
        return;
    }

    // ── Pre-validate every .lua file with luaL_loadfile (compile-only, no execution).
    // This catches syntax errors produced by mid-edit autosaves without running
    // broken code.  If any file is malformed we display the error in the status bar
    // and skip the reload until the user finishes editing.
    lua_State* L = ComponentDB::GetLuaState();
    if (L)
    {
        for (const auto& entry : std::filesystem::directory_iterator(script_dir))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".lua")
                continue;

            if (luaL_loadfile(L, entry.path().string().c_str()) != LUA_OK)
            {
                const char* err_msg = lua_tostring(L, -1);
                saved_message = err_msg ? std::string(err_msg)
                                        : ("Syntax error in " + entry.path().stem().string());
                saved_message_frames = 600;
                lua_pop(L, 1);
                return;   // wait for the file to be saved in a valid state
            }
            lua_pop(L, 1);   // discard compiled chunk — do NOT execute it
        }
    }

    const std::string scene_name = CurrentSceneName();
    const std::string selected_name = selected_entity ? selected_entity->GetName() : "";

    component_db->ClearComponentTypeCache();

    try
    {
        ReloadScene(scene_name);
    }
    catch (const std::exception& e)
    {
        saved_message = std::string("Lua error: ") + e.what();
        saved_message_frames = 600;
        return;
    }

    selected_entity.reset();
    if (!selected_name.empty())
    {
        for (const auto& actor : scene_db->GetActors())
        {
            if (actor && actor->GetName() == selected_name)
            {
                selected_entity = actor;
                break;
            }
        }
    }

    saved_message = "Lua scripts reloaded";
    saved_message_frames = 120;
}

SDL_Rect SceneEditor::GetGameViewportRect(SDL_Window* window) const
{
    int window_width = last_window_width;
    int window_height = last_window_height;
    if (window != nullptr)
    {
        SDL_GetWindowSize(window, &window_width, &window_height);
    }

    // Center the game at native 1:1 resolution inside the available editor area.
    const SDL_Rect center = GetEditorCenterAreaRect(window);
    const int view_w = std::max(1, render_resolution_x);
    const int view_h = std::max(1, render_resolution_y);
    const int view_x = center.x + (center.w - view_w) / 2;
    const int view_y = center.y + (center.h - view_h) / 2;

    return SDL_Rect{ view_x, view_y, view_w, view_h };
}

SDL_Rect SceneEditor::GetEditorCenterAreaRect(SDL_Window* window) const
{
    int window_width = last_window_width;
    int window_height = last_window_height;
    if (window != nullptr)
    {
        SDL_GetWindowSize(window, &window_width, &window_height);
    }
    const int x = game_view_fullscreen ? 0 : left_panel_width;
    const int y = top_bar_height;
    const int w = game_view_fullscreen
        ? std::max(1, window_width)
        : std::max(1, window_width - left_panel_width - right_panel_width);
    const int h = std::max(1, window_height - top_bar_height);
    return SDL_Rect{ x, y, w, h };
}

void SceneEditor::RenderImGuiDrawData(SDL_Renderer* renderer)
{
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data == nullptr || draw_data->TotalVtxCount == 0)
    {
        return;
    }

    SDL_Rect previous_clip_rect;
    SDL_RenderGetClipRect(renderer, &previous_clip_rect);
    const bool had_clip = SDL_RenderIsClipEnabled(renderer) == SDL_TRUE;

    for (int list_index = 0; list_index < draw_data->CmdListsCount; ++list_index)
    {
        const ImDrawList* draw_list = draw_data->CmdLists[list_index];
        const ImDrawVert* vertices = draw_list->VtxBuffer.Data;
        const ImDrawIdx* indices = draw_list->IdxBuffer.Data;

        int index_offset = 0;
        for (int command_index = 0; command_index < draw_list->CmdBuffer.Size; ++command_index)
        {
            const ImDrawCmd* command = &draw_list->CmdBuffer[command_index];
            if (command->UserCallback != nullptr)
            {
                command->UserCallback(draw_list, command);
                index_offset += static_cast<int>(command->ElemCount);
                continue;
            }

            SDL_Rect clip_rect;
            clip_rect.x = static_cast<int>(command->ClipRect.x);
            clip_rect.y = static_cast<int>(command->ClipRect.y);
            clip_rect.w = static_cast<int>(command->ClipRect.z - command->ClipRect.x);
            clip_rect.h = static_cast<int>(command->ClipRect.w - command->ClipRect.y);
            if (clip_rect.w <= 0 || clip_rect.h <= 0)
            {
                index_offset += static_cast<int>(command->ElemCount);
                continue;
            }
            SDL_RenderSetClipRect(renderer, &clip_rect);

            std::vector<SDL_Vertex> sdl_vertices;
            sdl_vertices.reserve(command->ElemCount);
            std::vector<int> sdl_indices;
            sdl_indices.reserve(command->ElemCount);

            for (unsigned int i = 0; i < command->ElemCount; ++i)
            {
                const ImDrawIdx draw_index = indices[index_offset + static_cast<int>(i)];
                const ImDrawVert& draw_vertex = vertices[draw_index];

                SDL_Vertex vertex;
                vertex.position.x = draw_vertex.pos.x;
                vertex.position.y = draw_vertex.pos.y;
                vertex.tex_coord.x = draw_vertex.uv.x;
                vertex.tex_coord.y = draw_vertex.uv.y;
                vertex.color.r = static_cast<Uint8>((draw_vertex.col >> IM_COL32_R_SHIFT) & 0xFF);
                vertex.color.g = static_cast<Uint8>((draw_vertex.col >> IM_COL32_G_SHIFT) & 0xFF);
                vertex.color.b = static_cast<Uint8>((draw_vertex.col >> IM_COL32_B_SHIFT) & 0xFF);
                vertex.color.a = static_cast<Uint8>((draw_vertex.col >> IM_COL32_A_SHIFT) & 0xFF);

                sdl_indices.push_back(static_cast<int>(sdl_vertices.size()));
                sdl_vertices.push_back(vertex);
            }

            SDL_Texture* texture = reinterpret_cast<SDL_Texture*>(command->TextureId);
            SDL_RenderGeometry(renderer, texture, sdl_vertices.data(), static_cast<int>(sdl_vertices.size()), sdl_indices.data(), static_cast<int>(sdl_indices.size()));
            index_offset += static_cast<int>(command->ElemCount);
        }
    }

    if (had_clip)
    {
        SDL_RenderSetClipRect(renderer, &previous_clip_rect);
    }
    else
    {
        SDL_RenderSetClipRect(renderer, nullptr);
    }
}

std::string SceneEditor::CurrentSceneName() const
{
    const std::string scene_name = SceneDB::LuaGetCurrentScene();
    if (!scene_name.empty())
    {
        return scene_name;
    }
    if (selected_scene_index >= 0 && selected_scene_index < static_cast<int>(scene_names.size()))
    {
        return scene_names[selected_scene_index];
    }
    return "scene0";
}

bool SceneEditor::IsBuiltinComponentType(const std::string& type_name) const
{
    return type_name == "Rigidbody" || type_name == "ParticleSystem";
}

bool SceneEditor::CreatePlayModeSceneBackup(const std::string& scene_name)
{
    if (scene_name.empty())
    {
        return false;
    }

    const std::filesystem::path scene_path = std::filesystem::path("resources/scenes") / (scene_name + ".scene");
    if (!std::filesystem::exists(scene_path))
    {
        return false;
    }

    const std::filesystem::path backup_dir = std::filesystem::path("resources/.editor_playbackups");
    std::error_code ec;
    std::filesystem::create_directories(backup_dir, ec);
    if (ec)
    {
        return false;
    }

    const std::filesystem::path backup_path = backup_dir / (scene_name + ".scene.playbackup");
    std::filesystem::copy_file(scene_path, backup_path, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec)
    {
        return false;
    }

    play_mode_scene_name = scene_name;
    play_mode_scene_backup_path = backup_path;
    return true;
}

void SceneEditor::RestorePlayModeSceneBackup()
{
    if (play_mode_scene_name.empty() || play_mode_scene_backup_path.empty())
    {
        play_mode_scene_name.clear();
        play_mode_scene_backup_path.clear();
        return;
    }

    const std::filesystem::path scene_path = std::filesystem::path("resources/scenes") / (play_mode_scene_name + ".scene");
    std::error_code ec;
    std::filesystem::copy_file(play_mode_scene_backup_path, scene_path, std::filesystem::copy_options::overwrite_existing, ec);
    std::filesystem::remove(play_mode_scene_backup_path, ec);

    play_mode_scene_name.clear();
    play_mode_scene_backup_path.clear();
}

bool SceneEditor::EnterPlayMode()
{
    if (is_playing)
    {
        return true;
    }

    const std::string scene_name = CurrentSceneName();
    if (scene_name.empty())
    {
        return false;
    }

    SaveScene(scene_name);
    if (!CreatePlayModeSceneBackup(scene_name))
    {
        saved_message = "Failed to enter Play (backup error)";
        saved_message_frames = 180;
        return false;
    }

    // Always reload component scripts from disk before playing so that any code
    // the user saved after the last hot-reload poll is picked up immediately.
    if (component_db != nullptr)
    {
        component_db->ClearComponentTypeCache();
    }

    try
    {
        ReloadScene(scene_name);
    }
    catch (const std::exception& e)
    {
        // Lua error in one of the component scripts — show the message in the
        // editor status bar and abort play mode rather than crashing.
        saved_message = std::string("Play error: ") + e.what();
        saved_message_frames = 600;
        is_playing = false;
        RestorePlayModeSceneBackup();
        if (component_db != nullptr) component_db->ClearComponentTypeCache();
        ReloadScene(scene_name);   // restore the editor scene (best-effort)
        return false;
    }

    is_playing = true;
    saved_message = "Play";
    saved_message_frames = 180;
    return true;
}

void SceneEditor::ExitPlayMode()
{
    if (!is_playing)
    {
        return;
    }

    const std::string scene_name = !play_mode_scene_name.empty() ? play_mode_scene_name : CurrentSceneName();

    if (scene_db != nullptr)
    {
        for (const auto& actor : scene_db->GetActors())
        {
            if (actor)
            {
                actor->dont_destroy_on_load = false;
            }
        }
    }

    Actor::FlushPendingQueues();
    SceneDB::FlushPendingQueues();

    is_playing = false;
    RestorePlayModeSceneBackup();
    ReloadScene(scene_name);
    selected_entity.reset();
    saved_message = "Stop";
    saved_message_frames = 180;
}

void SceneEditor::HandleShortcutKeyDown(const SDL_KeyboardEvent& key_event)
{
    if (key_event.repeat != 0)
    {
        return;
    }

    const SDL_Scancode code = key_event.keysym.scancode;
    const SDL_Keymod mods = SDL_GetModState();
    const bool ctrl_or_cmd = ((mods & KMOD_CTRL) != 0) || ((mods & KMOD_GUI) != 0);

    if (code == SDL_SCANCODE_F1)
    {
        enabled = !enabled;
        saved_message = enabled ? "Editor enabled" : "Editor hidden (F1 to show)";
        saved_message_frames = 180;
        return;
    }

    if (code == SDL_SCANCODE_F5)
    {
        if (!is_playing)
        {
            EnterPlayMode();
        }
        else
        {
            ExitPlayMode();
        }
        return;
    }

    if (ctrl_or_cmd && code == SDL_SCANCODE_S)
    {
        SaveScene(CurrentSceneName());
        RefreshSceneList();
        SelectSceneInList(CurrentSceneName());
        return;
    }
}

std::optional<std::pair<float, float>> SceneEditor::TryGetActorWorldPosition(const std::shared_ptr<Actor>& actor) const
{
    if (!actor)
    {
        return std::nullopt;
    }

    for (const auto& [component_key, component_ref_ptr] : actor->components)
    {
        (void)component_key;
        if (!component_ref_ptr)
        {
            continue;
        }
        luabridge::LuaRef comp = *component_ref_ptr;
        if (!comp.isTable() && !comp.isUserdata())
        {
            continue;
        }

        luabridge::LuaRef x_ref = comp["x"];
        luabridge::LuaRef y_ref = comp["y"];
        if (x_ref.isNumber() && y_ref.isNumber())
        {
            return std::make_pair(x_ref.cast<float>(), y_ref.cast<float>());
        }
    }

    return std::nullopt;
}

void SceneEditor::HandleViewportSelectionClick(int mouse_x, int mouse_y)
{
    if (scene_db == nullptr)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
    {
        return;
    }

    const SDL_Rect viewport = GetGameViewportRect(nullptr);
    if (mouse_x < viewport.x || mouse_x >= (viewport.x + viewport.w) ||
        mouse_y < viewport.y || mouse_y >= (viewport.y + viewport.h))
    {
        return;
    }

    const float local_x = static_cast<float>(mouse_x - viewport.x);
    const float local_y = static_cast<float>(mouse_y - viewport.y);

    const float camera_x = Renderer::LuaGetCameraPositionX();
    const float camera_y = Renderer::LuaGetCameraPositionY();
    const float zoom = std::max(0.0001f, Renderer::LuaGetCameraZoom());

    const float world_x = (local_x / zoom - static_cast<float>(render_resolution_x) * 0.5f / zoom) / pixels_per_meter + camera_x;
    const float world_y = (local_y / zoom - static_cast<float>(render_resolution_y) * 0.5f / zoom) / pixels_per_meter + camera_y;

    const float max_pick_distance_sq = 0.5f * 0.5f;
    float best_dist_sq = std::numeric_limits<float>::max();
    std::shared_ptr<Actor> best_actor = nullptr;

    for (const auto& actor : scene_db->GetActors())
    {
        auto pos = TryGetActorWorldPosition(actor);
        if (!pos.has_value())
        {
            continue;
        }

        const float dx = pos->first - world_x;
        const float dy = pos->second - world_y;
        const float dist_sq = dx * dx + dy * dy;
        if (dist_sq < max_pick_distance_sq && dist_sq < best_dist_sq)
        {
            best_dist_sq = dist_sq;
            best_actor = actor;
        }
    }

    if (best_actor)
    {
        selected_entity = best_actor;
    }
}

void SceneEditor::RefreshSceneList()
{
    scene_names.clear();

    const std::filesystem::path scenes_dir = "resources/scenes";
    if (std::filesystem::exists(scenes_dir) && std::filesystem::is_directory(scenes_dir))
    {
        for (const auto& entry : std::filesystem::directory_iterator(scenes_dir))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }
            const std::filesystem::path file_path = entry.path();
            if (file_path.extension() == ".scene")
            {
                scene_names.push_back(file_path.stem().string());
            }
        }
    }

    std::sort(scene_names.begin(), scene_names.end());
    scene_names.erase(std::unique(scene_names.begin(), scene_names.end()), scene_names.end());

    if (scene_names.empty())
    {
        scene_names.push_back(CurrentSceneName());
    }
}

void SceneEditor::SelectSceneInList(const std::string& scene_name)
{
    selected_scene_index = -1;
    for (int i = 0; i < static_cast<int>(scene_names.size()); ++i)
    {
        if (scene_names[i] == scene_name)
        {
            selected_scene_index = i;
            return;
        }
    }
}

void SceneEditor::RenderControlsBar()
{
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(last_window_width), static_cast<float>(top_bar_height)), ImGuiCond_Always);
    ImGui::Begin("Scene Controls", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    // --- Play / Stop ---
    if (!is_playing)
    {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.55f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.70f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.10f, 0.40f, 0.10f, 1.0f));
        if (ImGui::Button("  Play  "))
        {
            EnterPlayMode();
        }
        ImGui::PopStyleColor(3);
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.40f, 0.10f, 0.10f, 1.0f));
        if (ImGui::Button("  Stop  "))
        {
            ExitPlayMode();
        }
        ImGui::PopStyleColor(3);
    }
    ImGui::SameLine();

    // --- Entity tools ---
    ImGui::PushItemWidth(140.0f);
    ImGui::InputText("##EntityName", new_entity_name.data(), new_entity_name.size());
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Add Actor"))
    {
        CreateEntity(new_entity_name.data());
    }
    ImGui::SameLine();
    if (selected_entity && ImGui::Button("Delete Actor"))
    {
        DeleteEntity(selected_entity);
        selected_entity.reset();
    }
    ImGui::SameLine();

    // --- Scene persistence ---
    if (ImGui::Button("Save"))
    {
        SaveScene(CurrentSceneName());
        RefreshSceneList();
        SelectSceneInList(CurrentSceneName());
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload"))
    {
        ReloadScene(CurrentSceneName());
        selected_entity.reset();
        RefreshSceneList();
        SelectSceneInList(CurrentSceneName());
    }
    ImGui::SameLine();

    // --- Scene selector ---
    RefreshSceneList();
    if (selected_scene_index < 0 || selected_scene_index >= static_cast<int>(scene_names.size()))
    {
        SelectSceneInList(CurrentSceneName());
    }

    const char* scene_preview = (selected_scene_index >= 0 && selected_scene_index < static_cast<int>(scene_names.size()))
        ? scene_names[selected_scene_index].c_str()
        : "(none)";

    ImGui::PushItemWidth(140.0f);
    if (ImGui::BeginCombo("##SceneSelect", scene_preview))
    {
        for (int i = 0; i < static_cast<int>(scene_names.size()); ++i)
        {
            const bool is_selected = (selected_scene_index == i);
            if (ImGui::Selectable(scene_names[i].c_str(), is_selected))
            {
                selected_scene_index = i;
            }
            if (is_selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();

    ImGui::PushItemWidth(110.0f);
    ImGui::InputText("##NewSceneName", new_scene_name.data(), new_scene_name.size());
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("New Scene"))
    {
        std::string target_scene = new_scene_name.data();
        if (!target_scene.empty())
        {
            if (is_playing)
            {
                ExitPlayMode();
            }
            // Write a blank scene file — do NOT copy current actors
            std::filesystem::create_directories("resources/scenes");
            const std::filesystem::path new_scene_path =
                std::filesystem::path("resources/scenes") / (target_scene + ".scene");
            std::ofstream blank_out(new_scene_path);
            if (blank_out.is_open()) { blank_out << "{}"; blank_out.close(); }
            ReloadScene(target_scene);
            selected_entity.reset();
            RefreshSceneList();
            SelectSceneInList(target_scene);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Switch Scene"))
    {
        if (selected_scene_index >= 0 && selected_scene_index < static_cast<int>(scene_names.size()))
        {
            if (is_playing)
            {
                ExitPlayMode();
            }
            ReloadScene(scene_names[selected_scene_index]);
            selected_entity.reset();
            SelectSceneInList(scene_names[selected_scene_index]);
        }
    }
    ImGui::SameLine();

    if (ImGui::Button(game_view_fullscreen ? "Exit Fullscreen" : "Game Fullscreen"))
    {
        game_view_fullscreen = !game_view_fullscreen;
    }
    ImGui::SameLine();

    // --- Quit (top-right icon) ---
    ImGui::SetCursorPosX(std::max(0.0f, ImGui::GetWindowWidth() - 34.0f));
    ImGui::SetCursorPosY(8.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.45f, 0.10f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.30f, 0.05f, 0.05f, 1.0f));
    if (ImGui::Button("X", ImVec2(24.0f, 24.0f)))
    {
        quit_requested = true;
    }
    ImGui::PopStyleColor(3);

    if (!saved_message.empty())
    {
        ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), 40.0f));
        ImGui::TextUnformatted(saved_message.c_str());
    }

    ImGui::End();
}

void SceneEditor::RenderHierarchyPanel()
{
    ImGui::SetNextWindowPos(ImVec2(0.0f, static_cast<float>(top_bar_height)), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(left_panel_width), static_cast<float>(std::max(1, last_window_height - top_bar_height))), ImGuiCond_Always);
    ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    auto& actors = scene_db->GetActors();
    for (const auto& actor : actors)
    {
        if (!actor) continue;

        const bool is_selected = (selected_entity == actor);
        const bool is_renaming = (renaming_actor == actor);

        if (is_renaming)
        {
            ImGui::PushID(actor.get());
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 4.0f);
            ImGui::SetKeyboardFocusHere();
            if (ImGui::InputText("##rename", rename_buf.data(), rename_buf.size(),
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
            {
                std::string new_name = TrimString(rename_buf.data());
                if (!new_name.empty())
                {
                    actor->actor_name = new_name;
                    if (!CurrentSceneName().empty()) SaveScene(CurrentSceneName());
                }
                renaming_actor = nullptr;
            }
            if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(SDL_SCANCODE_ESCAPE))
            {
                renaming_actor = nullptr;
            }
            else if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0) && !ImGui::IsItemHovered())
            {
                renaming_actor = nullptr;
            }
            ImGui::PopItemWidth();
            ImGui::PopID();
        }
        else
        {
            if (ImGui::Selectable(actor->GetName().c_str(), is_selected))
            {
                selected_entity = actor;
            }
            // Double-click → enter rename mode
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            {
                renaming_actor = actor;
                std::string cur = actor->GetName();
                std::fill(rename_buf.begin(), rename_buf.end(), '\0');
                std::copy(cur.begin(),
                          cur.begin() + std::min(cur.size(), rename_buf.size() - 1),
                          rename_buf.begin());
            }
            // Select + Enter → enter rename mode (only when Hierarchy window itself has focus,
            // so that confirming an InputText in the Inspector doesn't also trigger rename)
            if (is_selected && ImGui::IsKeyPressed(SDL_SCANCODE_RETURN) && !renaming_actor && ImGui::IsWindowFocused())
            {
                renaming_actor = actor;
                std::string cur = actor->GetName();
                std::fill(rename_buf.begin(), rename_buf.end(), '\0');
                std::copy(cur.begin(),
                          cur.begin() + std::min(cur.size(), rename_buf.size() - 1),
                          rename_buf.begin());
            }
        }
    }

    ImGui::End();
}

bool SceneEditor::TryGetNumber(const luabridge::LuaRef& table, const char* key, float& out_value)
{
    if (!table.isTable())
    {
        return false;
    }
    luabridge::LuaRef value_ref = table[key];
    if (!value_ref.isNumber())
    {
        return false;
    }
    out_value = value_ref.cast<float>();
    return true;
}

void SceneEditor::SyncActorTransformPosition(const std::shared_ptr<Actor>& actor)
{
    if (!actor)
    {
        return;
    }

    std::shared_ptr<luabridge::LuaRef> transform = FindComponentByType(actor, "Transform");
    if (!transform)
    {
        return;
    }

    float tx = 0.0f;
    float ty = 0.0f;
    if (!TryReadComponentXY(*transform, tx, ty))
    {
        return;
    }

    if (std::shared_ptr<luabridge::LuaRef> sprite = FindComponentByType(actor, "SpriteRenderer"))
    {
        WriteComponentXY(*sprite, tx, ty);
    }
    if (std::shared_ptr<luabridge::LuaRef> rigidbody = FindComponentByType(actor, "Rigidbody"))
    {
        WriteComponentXY(*rigidbody, tx, ty);
    }
}

void SceneEditor::RenderComponentProperties(luabridge::LuaRef& comp_ref, const std::string& comp_key)
{
    lua_State* L = ComponentDB::GetLuaState();
    if (!L || !comp_ref.isTable())
    {
        return;
    }

    // Keys that are Lua lifecycle / system fields — not user-editable properties
    static const std::unordered_set<std::string> kSkipKeys = {
        "OnStart", "OnUpdate", "OnLateUpdate", "OnDestroy",
        "OnCollisionEnter", "OnCollisionExit", "OnTriggerEnter", "OnTriggerExit",
        "key", "actor", "type", "enabled"
    };

    bool hide_sprite_position = false;
    bool is_template_spawner = false;
    luabridge::LuaRef type_ref = comp_ref["type"];
    if (type_ref.isString())
    {
        const std::string component_type = type_ref.cast<std::string>();
        hide_sprite_position = (component_type == "SpriteRenderer");
        is_template_spawner = (component_type == "TemplateSpawner");
    }

    std::vector<std::string> saved_template_names;
    if (is_template_spawner)
    {
        const std::filesystem::path template_dir("resources/actor_templates");
        if (std::filesystem::exists(template_dir))
        {
            for (const auto& entry : std::filesystem::directory_iterator(template_dir))
            {
                if (!entry.is_regular_file() || entry.path().extension() != ".template")
                {
                    continue;
                }
                saved_template_names.push_back(entry.path().stem().string());
            }
            std::sort(saved_template_names.begin(), saved_template_names.end());
        }
    }

    // Collect all editable properties first (can't nest ImGui calls inside lua_next safely)
    struct Prop { std::string key; int lua_type; };
    std::vector<Prop> props;
    std::unordered_set<std::string> seen_keys;

    // Pass 1: direct properties on the instance table
    comp_ref.push(L);
    lua_pushnil(L);
    while (lua_next(L, -2) != 0)
    {
        if (lua_type(L, -2) == LUA_TSTRING)
        {
            std::string k(lua_tostring(L, -2));
            if (kSkipKeys.find(k) == kSkipKeys.end())
            {
                if (hide_sprite_position && (k == "x" || k == "y"))
                {
                    lua_pop(L, 1);
                    continue;
                }
                int vt = lua_type(L, -1);
                if (vt == LUA_TNUMBER || vt == LUA_TSTRING || vt == LUA_TBOOLEAN)
                {
                    props.push_back({k, vt});
                    seen_keys.insert(k);
                }
            }
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    // Pass 2: class-level defaults from the metatable's __index (e.g. frames_until_hop = 3)
    comp_ref.push(L);  // push instance
    if (lua_getmetatable(L, -1))  // push metatable (if any)
    {
        lua_getfield(L, -1, "__index");  // push __index table
        if (lua_istable(L, -1))
        {
            lua_pushnil(L);
            while (lua_next(L, -2) != 0)
            {
                if (lua_type(L, -2) == LUA_TSTRING)
                {
                    std::string k(lua_tostring(L, -2));
                    if (kSkipKeys.find(k) == kSkipKeys.end() && seen_keys.find(k) == seen_keys.end())
                    {
                        if (hide_sprite_position && (k == "x" || k == "y"))
                        {
                            lua_pop(L, 1);
                            continue;
                        }
                        int vt = lua_type(L, -1);
                        if (vt == LUA_TNUMBER || vt == LUA_TSTRING || vt == LUA_TBOOLEAN)
                        {
                            props.push_back({k, vt});
                            seen_keys.insert(k);
                        }
                    }
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);  // pop __index
        lua_pop(L, 1);  // pop metatable
    }
    lua_pop(L, 1);  // pop instance

    std::sort(props.begin(), props.end(), [](const Prop& a, const Prop& b) { return a.key < b.key; });

    for (auto& prop : props)
    {
        std::string widget_id = comp_key + "_" + prop.key;
        ImGui::PushID(widget_id.c_str());
        if (prop.lua_type == LUA_TNUMBER)
        {
            float val = comp_ref[prop.key].cast<float>();
            static std::unordered_set<std::string> s_float_edit_mode;
            static std::unordered_set<std::string> s_float_edit_focus;
            const bool in_edit = s_float_edit_mode.count(widget_id) > 0;
            if (in_edit)
            {
                if (s_float_edit_focus.count(widget_id))
                {
                    ImGui::SetKeyboardFocusHere();
                    s_float_edit_focus.erase(widget_id);
                }
                bool confirmed = ImGui::InputFloat(prop.key.c_str(), &val, 0.0f, 0.0f, "%.4g",
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
                if (confirmed)
                {
                    comp_ref[prop.key] = val;
                    s_float_edit_mode.erase(widget_id);
                }
                else if (ImGui::IsItemDeactivated())
                {
                    // Commit whatever was typed when focus is lost
                    comp_ref[prop.key] = val;
                    s_float_edit_mode.erase(widget_id);
                }
            }
            else
            {
                if (ImGui::DragFloat(prop.key.c_str(), &val, 0.1f))
                {
                    comp_ref[prop.key] = val;
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                {
                    s_float_edit_mode.insert(widget_id);
                    s_float_edit_focus.insert(widget_id);
                }
            }
        }
        else if (prop.lua_type == LUA_TBOOLEAN)
        {
            bool val = comp_ref[prop.key].cast<bool>();
            if (ImGui::Checkbox(prop.key.c_str(), &val))
            {
                comp_ref[prop.key] = val;
            }
        }
        else if (prop.lua_type == LUA_TSTRING)
        {
            std::string val = comp_ref[prop.key].cast<std::string>();

            if (is_template_spawner && prop.key == "template_name" && !saved_template_names.empty())
            {
                int selected_index = 0;
                for (int i = 0; i < static_cast<int>(saved_template_names.size()); ++i)
                {
                    if (saved_template_names[i] == val)
                    {
                        selected_index = i;
                        break;
                    }
                }

                const char* preview = saved_template_names[selected_index].c_str();
                if (ImGui::BeginCombo(prop.key.c_str(), preview))
                {
                    for (int i = 0; i < static_cast<int>(saved_template_names.size()); ++i)
                    {
                        const bool is_selected = (selected_index == i);
                        if (ImGui::Selectable(saved_template_names[i].c_str(), is_selected))
                        {
                            comp_ref[prop.key] = saved_template_names[i];
                            if (!CurrentSceneName().empty())
                            {
                                SaveScene(CurrentSceneName());
                            }
                        }
                        if (is_selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopID();
                continue;
            }

            std::vector<char> buf(val.begin(), val.end());
            buf.resize(256, '\0');
            if (ImGui::InputText(prop.key.c_str(), buf.data(), buf.size()))
            {
                std::string new_val = std::string(buf.data());
                if (prop.key == "sprite" || prop.key == "image")
                {
                    std::filesystem::path maybe_path(new_val);
                    if (std::filesystem::exists(maybe_path) && maybe_path.has_extension())
                    {
                        std::optional<std::string> imported = ImportImageFileToProject(maybe_path);
                        if (imported.has_value())
                        {
                            new_val = imported.value();
                            saved_message = "Imported image: " + new_val;
                            saved_message_frames = 180;
                        }
                    }
                }
                else if (prop.key == "clip")
                {
                    std::filesystem::path maybe_path(new_val);
                    if (std::filesystem::exists(maybe_path) && maybe_path.has_extension())
                    {
                        std::optional<std::string> imported = ImportAudioFileToProject(maybe_path);
                        if (imported.has_value())
                        {
                            new_val = imported.value();
                            saved_message = "Imported audio: " + new_val;
                            saved_message_frames = 180;
                        }
                    }
                }
                comp_ref[prop.key] = new_val;
                if (!CurrentSceneName().empty())
                {
                    SaveScene(CurrentSceneName());
                }
            }

            if (prop.key == "sprite" || prop.key == "image")
            {
                ImGui::SameLine();
                std::string pick_label = "Pick##" + widget_id;
                if (ImGui::SmallButton(pick_label.c_str()))
                {
                    std::optional<std::string> imported = PickImageAndImportToProject();
                    if (imported.has_value())
                    {
                        comp_ref[prop.key] = imported.value();
                        saved_message = "Imported image: " + imported.value();
                        saved_message_frames = 180;
                        if (!CurrentSceneName().empty())
                        {
                            SaveScene(CurrentSceneName());
                        }
                    }
                    else
                    {
                        saved_message = "Image import canceled/failed";
                        saved_message_frames = 180;
                    }
                }
            }
            else if (prop.key == "clip")
            {
                ImGui::SameLine();
                std::string pick_label = "Pick##" + widget_id;
                if (ImGui::SmallButton(pick_label.c_str()))
                {
                    std::optional<std::string> imported = PickAudioAndImportToProject();
                    if (imported.has_value())
                    {
                        comp_ref[prop.key] = imported.value();
                        saved_message = "Imported audio: " + imported.value();
                        saved_message_frames = 180;
                        if (!CurrentSceneName().empty())
                        {
                            SaveScene(CurrentSceneName());
                        }
                    }
                    else
                    {
                        saved_message = "Audio import canceled/failed";
                        saved_message_frames = 180;
                    }
                }
            }
        }
        ImGui::PopID();
    }
}

void SceneEditor::RenderBuiltinComponentProperties(luabridge::LuaRef& comp_ref, const std::string& type_name, const std::string& comp_key)
{
    static std::unordered_set<std::string> s_builtin_edit_mode;
    static std::unordered_set<std::string> s_builtin_edit_focus;

    auto edit_float = [&](const char* prop, float speed)
    {
        luabridge::LuaRef ref = comp_ref[prop];
        if (!ref.isNumber()) return;
        float value = ref.cast<float>();
        const std::string id = comp_key + "_" + prop;
        ImGui::PushID(id.c_str());
        const bool in_edit = s_builtin_edit_mode.count(id) > 0;
        if (in_edit)
        {
            if (s_builtin_edit_focus.count(id))
            {
                ImGui::SetKeyboardFocusHere();
                s_builtin_edit_focus.erase(id);
            }
            bool confirmed = ImGui::InputFloat(prop, &value, 0.0f, 0.0f, "%.4g",
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            if (confirmed)
            {
                comp_ref[prop] = value;
                s_builtin_edit_mode.erase(id);
            }
            else if (ImGui::IsItemDeactivated())
            {
                comp_ref[prop] = value;
                s_builtin_edit_mode.erase(id);
            }
        }
        else
        {
            if (ImGui::DragFloat(prop, &value, speed))
            {
                comp_ref[prop] = value;
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            {
                s_builtin_edit_mode.insert(id);
                s_builtin_edit_focus.insert(id);
            }
        }
        ImGui::PopID();
    };

    auto edit_bool = [&](const char* prop)
    {
        luabridge::LuaRef ref = comp_ref[prop];
        if (!ref.isBool()) return;
        bool value = ref.cast<bool>();
        std::string id = comp_key + "_" + prop;
        ImGui::PushID(id.c_str());
        if (ImGui::Checkbox(prop, &value))
        {
            comp_ref[prop] = value;
        }
        ImGui::PopID();
    };

    auto edit_string = [&](const char* prop)
    {
        luabridge::LuaRef ref = comp_ref[prop];
        if (!ref.isString()) return;
        std::string value = ref.cast<std::string>();
        std::vector<char> buf(value.begin(), value.end());
        buf.resize(256, '\0');
        std::string id = comp_key + "_" + prop;
        ImGui::PushID(id.c_str());
        if (ImGui::InputText(prop, buf.data(), buf.size()))
        {
            comp_ref[prop] = std::string(buf.data());
        }
        ImGui::PopID();
    };

    if (type_name == "Rigidbody")
    {
        edit_float("x", 0.1f);
        edit_float("y", 0.1f);
        edit_string("body_type");
        edit_bool("precise");
        edit_float("gravity_scale", 0.1f);
        edit_float("density", 0.1f);
        edit_float("angular_friction", 0.05f);
        edit_float("rotation", 1.0f);
        edit_string("collider_type");
        edit_float("width", 0.05f);
        edit_float("height", 0.05f);
        edit_float("radius", 0.05f);
        edit_string("trigger_type");
        edit_float("trigger_width", 0.05f);
        edit_float("trigger_height", 0.05f);
        edit_float("trigger_radius", 0.05f);
        edit_float("friction", 0.05f);
        edit_float("bounciness", 0.05f);
        edit_bool("has_collider");
        edit_bool("has_trigger");
    }
    else if (type_name == "ParticleSystem")
    {
        edit_float("x", 0.1f);
        edit_float("y", 0.1f);
        edit_float("emit_angle_min", 1.0f);
        edit_float("emit_angle_max", 1.0f);
        edit_float("emit_radius_min", 0.05f);
        edit_float("emit_radius_max", 0.05f);
        edit_float("start_scale_min", 0.05f);
        edit_float("start_scale_max", 0.05f);
        edit_float("rotation_min", 1.0f);
        edit_float("rotation_max", 1.0f);
        edit_float("start_speed_min", 0.05f);
        edit_float("start_speed_max", 0.05f);
        edit_float("rotation_speed_min", 0.1f);
        edit_float("rotation_speed_max", 0.1f);
        edit_float("gravity_scale_x", 0.05f);
        edit_float("gravity_scale_y", 0.05f);
        edit_float("drag_factor", 0.01f);
        edit_float("angular_drag_factor", 0.01f);
        edit_float("end_scale", 0.05f);

        // image field — same as SpriteRenderer sprite: show InputText + Pick button
        {
            luabridge::LuaRef ref = comp_ref["image"];
            if (ref.isString())
            {
                std::string img_val = ref.cast<std::string>();
                std::vector<char> img_buf(img_val.begin(), img_val.end());
                img_buf.resize(256, '\0');
                const std::string img_id = comp_key + "_image";
                ImGui::PushID(img_id.c_str());
                if (ImGui::InputText("image", img_buf.data(), img_buf.size()))
                {
                    std::string new_img = std::string(img_buf.data());
                    std::filesystem::path maybe_path(new_img);
                    if (std::filesystem::exists(maybe_path) && maybe_path.has_extension())
                    {
                        std::optional<std::string> imported = ImportImageFileToProject(maybe_path);
                        if (imported.has_value()) new_img = imported.value();
                    }
                    comp_ref["image"] = new_img;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Pick##ps_img"))
                {
                    std::optional<std::string> imported = PickImageAndImportToProject();
                    if (imported.has_value())
                    {
                        comp_ref["image"] = imported.value();
                        if (!CurrentSceneName().empty()) SaveScene(CurrentSceneName());
                    }
                }
                ImGui::PopID();
            }
        }
    }
}

void SceneEditor::RenderInspectorPanel()
{
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(std::max(0, last_window_width - right_panel_width)), static_cast<float>(top_bar_height)), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(right_panel_width), static_cast<float>(std::max(1, last_window_height - top_bar_height))), ImGuiCond_Always);
    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    if (!selected_entity)
    {
        ImGui::TextUnformatted("No actor selected");
        ImGui::End();
        return;
    }

    ImGui::Text("Actor: %s", selected_entity->GetName().c_str());
    ImGui::Separator();

    ImGui::TextUnformatted("Template");
    ImGui::PushItemWidth(180.0f);
    ImGui::InputText("##TemplateName", template_name_buf.data(), template_name_buf.size());
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Save Template"))
    {
        std::string name = TrimString(template_name_buf.data());
        if (name.empty())
        {
            name = selected_entity->GetName();
        }
        if (SaveActorAsTemplate(selected_entity, name))
        {
            std::fill(template_name_buf.begin(), template_name_buf.end(), '\0');
            saved_message = "Saved template: " + name;
            saved_message_frames = 180;
        }
        else
        {
            saved_message = "Failed to save template";
            saved_message_frames = 180;
        }
    }

    std::vector<std::string> template_names;
    const std::filesystem::path template_dir("resources/actor_templates");
    if (std::filesystem::exists(template_dir))
    {
        for (const auto& entry : std::filesystem::directory_iterator(template_dir))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".template")
            {
                continue;
            }
            template_names.push_back(entry.path().stem().string());
        }
        std::sort(template_names.begin(), template_names.end());
    }

    if (!template_names.empty())
    {
        if (selected_template_index < 0 || selected_template_index >= static_cast<int>(template_names.size()))
        {
            selected_template_index = 0;
        }

        ImGui::PushItemWidth(180.0f);
        const char* template_preview = template_names[selected_template_index].c_str();
        if (ImGui::BeginCombo("##InstantiateTemplateSelect", template_preview))
        {
            for (int i = 0; i < static_cast<int>(template_names.size()); ++i)
            {
                const bool is_selected = (selected_template_index == i);
                if (ImGui::Selectable(template_names[i].c_str(), is_selected))
                {
                    selected_template_index = i;
                }
                if (is_selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Instantiate Template"))
        {
            InstantiateTemplateInEditor(template_names[selected_template_index]);
        }
    }
    else
    {
        ImGui::TextDisabled("No saved templates found");
    }

    ImGui::Separator();

    // --- Components list ---
    std::vector<std::string> keys_to_remove;
    std::vector<std::string> removed_types;
    for (auto& [comp_key, comp_ref_ptr] : selected_entity->components)
    {
        if (!comp_ref_ptr) { continue; }
        luabridge::LuaRef& comp_ref = *comp_ref_ptr;

        std::string type_name = comp_key;
        luabridge::LuaRef type_field = comp_ref["type"];
        if (type_field.isString()) { type_name = type_field.cast<std::string>(); }

        std::string header_label = type_name + " [" + comp_key + "]";
        bool open = ImGui::CollapsingHeader(header_label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        if (open)
        {
            ImGui::PushID(comp_key.c_str());

            // Code button — open the Lua script in the system editor
            if (!IsBuiltinComponentType(type_name))
            {
                const std::filesystem::path code_path =
                    std::filesystem::path("resources/component_types") / (type_name + ".lua");
                if (ImGui::SmallButton("Code"))
                {
                    if (!std::filesystem::exists(code_path) && IsValidLuaIdentifier(type_name))
                    {
                        std::filesystem::create_directories(code_path.parent_path());
                        std::ofstream lua_file(code_path);
                        if (lua_file.is_open())
                        {
                            lua_file << BuildComponentTemplate(type_name);
                        }
                    }
                    system(("open \"" + std::filesystem::absolute(code_path).string() + "\"").c_str());
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%s", code_path.filename().string().c_str());
                ImGui::Spacing();
            }

            // enabled checkbox
            bool is_enabled = true;
            luabridge::LuaRef enabled_ref = comp_ref["enabled"];
            if (enabled_ref.isBool()) { is_enabled = enabled_ref.cast<bool>(); }
            if (ImGui::Checkbox("enabled", &is_enabled))
            {
                comp_ref["enabled"] = is_enabled;
            }

            if (comp_ref.isUserdata())
            {
                RenderBuiltinComponentProperties(comp_ref, type_name, comp_key);
            }
            else
            {
                RenderComponentProperties(comp_ref, comp_key);
            }

            // remove button
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.4f, 0.05f, 0.05f, 1.0f));
            if (ImGui::Button("Remove Component"))
            {
                keys_to_remove.push_back(comp_key);
                removed_types.push_back(type_name);
            }
            ImGui::PopStyleColor(3);

            ImGui::PopID();
        }
    }

    // Apply removals
    for (const auto& k : keys_to_remove)
    {
        auto found = selected_entity->components.find(k);
        if (found != selected_entity->components.end() && found->second)
        {
            selected_entity->RemoveComponent(*(found->second));
        }
    }
    if (!keys_to_remove.empty())
    {
        selected_entity->RebuildLifecycleComponentCaches();
        selected_entity->RebuildTypeComponentIndex();

        std::sort(removed_types.begin(), removed_types.end());
        removed_types.erase(std::unique(removed_types.begin(), removed_types.end()), removed_types.end());
        for (const std::string& removed_type : removed_types)
        {
            if (IsBuiltinComponentType(removed_type))
            {
                continue;
            }

            bool still_used = false;
            for (const auto& actor : scene_db->GetActors())
            {
                if (!actor)
                {
                    continue;
                }
                for (const auto& [key, comp_ptr] : actor->components)
                {
                    (void)key;
                    if (!comp_ptr)
                    {
                        continue;
                    }
                    luabridge::LuaRef type_ref = (*comp_ptr)["type"];
                    if (type_ref.isString() && type_ref.cast<std::string>() == removed_type)
                    {
                        still_used = true;
                        break;
                    }
                }
                if (still_used)
                {
                    break;
                }
            }

            if (!still_used)
            {
                const std::filesystem::path lua_path =
                    std::filesystem::path("resources/component_types") / (removed_type + ".lua");
                std::error_code ec;
                std::filesystem::remove(lua_path, ec);
            }
        }

        if (!CurrentSceneName().empty())
        {
            SaveScene(CurrentSceneName());
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Add Component");
    EnsureEngineLuaBuiltinScripts();
    RefreshCustomComponentTypeList();

    static const std::vector<std::string> kBuiltinChoices = {
        "SpriteRenderer", "CameraManager", "Transform",
        "ConstantMovement", "KeyboardControl", "PlayAudio", "TemplateSpawner",
        "Rigidbody", "ParticleSystem"
    };
    if (selected_builtin_component_index < 0 || selected_builtin_component_index >= static_cast<int>(kBuiltinChoices.size()))
    {
        selected_builtin_component_index = 0;
    }

    ImGui::TextUnformatted("Built-in");
    const char* builtin_preview = kBuiltinChoices[selected_builtin_component_index].c_str();
    ImGui::PushItemWidth(180.0f);
    if (ImGui::BeginCombo("##BuiltinCompSelect", builtin_preview))
    {
        for (int i = 0; i < static_cast<int>(kBuiltinChoices.size()); ++i)
        {
            const bool is_selected = (selected_builtin_component_index == i);
            if (ImGui::Selectable(kBuiltinChoices[i].c_str(), is_selected))
            {
                selected_builtin_component_index = i;
            }
            if (is_selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Add Built-in"))
    {
        AddComponentToActor(selected_entity, kBuiltinChoices[selected_builtin_component_index]);
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Custom");
    if (!custom_component_types.empty())
    {
        const char* custom_preview = custom_component_types[selected_custom_component_index].c_str();
        ImGui::PushItemWidth(180.0f);
        if (ImGui::BeginCombo("##CustomCompSelect", custom_preview))
        {
            for (int i = 0; i < static_cast<int>(custom_component_types.size()); ++i)
            {
                const bool is_selected = (selected_custom_component_index == i);
                if (ImGui::Selectable(custom_component_types[i].c_str(), is_selected))
                {
                    selected_custom_component_index = i;
                }
                if (is_selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Add Custom"))
        {
            AddComponentToActor(selected_entity, custom_component_types[selected_custom_component_index]);
        }
    }
    else
    {
        ImGui::TextDisabled("No custom components found");
    }

    ImGui::PushItemWidth(180.0f);
    ImGui::InputText("##NewCompType", new_component_type.data(), new_component_type.size());
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Create + Add"))
    {
        std::string type_name = NormalizeComponentTypeName(new_component_type.data());
        if (!type_name.empty())
        {
            AddComponentToActor(selected_entity, type_name);
            std::fill(new_component_type.begin(), new_component_type.end(), '\0');
            RefreshCustomComponentTypeList();
        }
    }

    if (!is_playing)
    {
        SyncActorTransformPosition(selected_entity);
    }

    ImGui::End();
}

void SceneEditor::CreateEntity(const std::string& name)
{
    if (scene_db == nullptr)
    {
        return;
    }

    // Ensure resource directories exist before creating entity
    std::filesystem::create_directories("resources/scenes");

    const std::string entity_name = name.empty() ? "Entity" : name;
    std::shared_ptr<Actor> actor = std::make_shared<Actor>();
    actor->actor_name = entity_name;
    scene_db->GetActors().push_back(actor);

    EnsureEngineLuaBuiltinScripts();
    if (component_db != nullptr)
    {
        component_db->CreateRuntimeComponent(actor.get(), "Transform");
    }

    selected_entity = actor;

    // Auto-save after creating entity so resources are persisted
    if (!CurrentSceneName().empty())
    {
        SaveScene(CurrentSceneName());
    }

    saved_message = "Created entity: " + entity_name;
    saved_message_frames = 180;
}

void SceneEditor::DeleteEntity(std::shared_ptr<Actor> actor)
{
    if (scene_db == nullptr || !actor)
    {
        return;
    }

    // Route deletion through SceneDB's lifecycle so components/physics are cleaned safely.
    SceneDB::DestroyActor(actor.get());

    saved_message = "Deleted entity";
    saved_message_frames = 180;
}

void SceneEditor::AddComponentToActor(std::shared_ptr<Actor> actor, const std::string& type_name)
{
    const std::string normalized_type_name = NormalizeComponentTypeName(type_name);
    if (!actor || normalized_type_name.empty() || component_db == nullptr)
    {
        return;
    }

    // Ensure resource directories exist
    std::filesystem::create_directories("resources/component_types");

    const bool needs_transform =
        normalized_type_name == "KeyboardControl"
        || normalized_type_name == "ConstantMovement"
        || normalized_type_name == "SpriteRenderer"
        || normalized_type_name == "Rigidbody";

    if (needs_transform)
    {
        std::shared_ptr<luabridge::LuaRef> transform = FindComponentByType(actor, "Transform");
        if (!transform)
        {
            EnsureEngineLuaBuiltinScripts();
            component_db->CreateRuntimeComponent(actor.get(), "Transform");
            transform = FindComponentByType(actor, "Transform");
            if (transform)
            {
                if (std::optional<std::pair<float, float>> p = GetPreferredActorPosition(actor))
                {
                    WriteComponentXY(*transform, p->first, p->second);
                }
            }
        }
    }

    const std::filesystem::path component_path = std::filesystem::path("resources/component_types") / (normalized_type_name + ".lua");
    if (!IsBuiltinComponentType(normalized_type_name) && !std::filesystem::exists(component_path))
    {
        if (!IsValidLuaIdentifier(normalized_type_name))
        {
            saved_message = "Invalid component name (use letters/numbers/_ and not starting with number): " + normalized_type_name;
            saved_message_frames = 300;
            return;
        }

        std::filesystem::create_directories(component_path.parent_path());
        std::ofstream lua_file(component_path);
        if (!lua_file.is_open())
        {
            saved_message = "Failed to create component script: " + component_path.string();
            saved_message_frames = 300;
            return;
        }
        if (IsEngineLuaBuiltinComponent(normalized_type_name))
        {
            lua_file << GetEngineLuaBuiltinScript(normalized_type_name);
        }
        else
        {
            lua_file << BuildComponentTemplate(normalized_type_name);
        }
        lua_file.close();

        // Register with an epoch timestamp so the watcher always treats the next real
        // mtime as a "change" — this way, even if the user saves edits within the first
        // watcher poll window, a hot-reload is guaranteed to fire.
        lua_component_write_times[normalized_type_name] = std::filesystem::file_time_type{};

        // Open the newly created script in the default editor
        system(("open \"" + std::filesystem::absolute(component_path).string() + "\"").c_str());

        saved_message = "Created script: " + component_path.string();
        saved_message_frames = 240;
    }

    const size_t before_count = actor->components.size();
    component_db->CreateRuntimeComponent(actor.get(), normalized_type_name);

    if (actor->components.size() <= before_count)
    {
        saved_message = "Failed to add component: " + normalized_type_name;
        saved_message_frames = 240;
        return;
    }

    if (!is_playing)
    {
        SyncActorTransformPosition(actor);
    }

    if (!CurrentSceneName().empty())
    {
        SaveScene(CurrentSceneName());
    }

    saved_message = "Added component: " + normalized_type_name;
    saved_message_frames = 180;
}

bool SceneEditor::SaveActorAsTemplate(const std::shared_ptr<Actor>& actor, const std::string& template_name)
{
    if (!actor || template_name.empty())
    {
        return false;
    }

    std::string normalized = NormalizeComponentTypeName(template_name);
    if (!IsValidLuaIdentifier(normalized))
    {
        return false;
    }

    std::filesystem::create_directories("resources/actor_templates");

    rapidjson::Document actor_doc;
    actor_doc.SetObject();
    auto& allocator = actor_doc.GetAllocator();

    static const std::unordered_set<std::string> kSkipSaveKeys = {
        "OnStart", "OnUpdate", "OnLateUpdate", "OnDestroy",
        "OnCollisionEnter", "OnCollisionExit", "OnTriggerEnter", "OnTriggerExit",
        "key", "actor"
    };

    rapidjson::Value name_json;
    const std::string actor_name = actor->GetName();
    name_json.SetString(actor_name.c_str(), static_cast<rapidjson::SizeType>(actor_name.size()), allocator);
    actor_doc.AddMember("name", name_json, allocator);

    rapidjson::Value components_json(rapidjson::kObjectType);
    for (const auto& [component_key, component_ref_ptr] : actor->components)
    {
        if (!component_ref_ptr)
        {
            continue;
        }
        luabridge::LuaRef component_ref = *component_ref_ptr;
        rapidjson::Value component_json(rapidjson::kObjectType);

        auto add_number = [&](const char* key_name)
        {
            luabridge::LuaRef ref = component_ref[key_name];
            if (!ref.isNumber()) return;
            const float value = static_cast<float>(ref.cast<float>());
            if (!std::isfinite(value)) return;
            rapidjson::Value k;
            k.SetString(key_name, allocator);
            component_json.AddMember(k, value, allocator);
        };

        auto add_int = [&](const char* key_name)
        {
            luabridge::LuaRef ref = component_ref[key_name];
            if (!ref.isNumber()) return;
            rapidjson::Value k;
            k.SetString(key_name, allocator);
            component_json.AddMember(k, ref.cast<int>(), allocator);
        };

        auto add_bool = [&](const char* key_name)
        {
            luabridge::LuaRef ref = component_ref[key_name];
            if (!ref.isBool()) return;
            rapidjson::Value k;
            k.SetString(key_name, allocator);
            component_json.AddMember(k, ref.cast<bool>(), allocator);
        };

        auto add_string = [&](const char* key_name)
        {
            luabridge::LuaRef ref = component_ref[key_name];
            if (!ref.isString()) return;
            rapidjson::Value k;
            k.SetString(key_name, allocator);
            rapidjson::Value v;
            const std::string s = ref.cast<std::string>();
            v.SetString(s.c_str(), static_cast<rapidjson::SizeType>(s.size()), allocator);
            component_json.AddMember(k, v, allocator);
        };

        lua_State* L = ComponentDB::GetLuaState();
        if (component_ref.isTable() && L)
        {
            component_ref.push(L);
            lua_pushnil(L);
            while (lua_next(L, -2) != 0)
            {
                if (lua_type(L, -2) == LUA_TSTRING)
                {
                    std::string k(lua_tostring(L, -2));
                    if (kSkipSaveKeys.find(k) == kSkipSaveKeys.end())
                    {
                        int vt = lua_type(L, -1);
                        rapidjson::Value key_json;
                        key_json.SetString(k.c_str(), static_cast<rapidjson::SizeType>(k.size()), allocator);
                        if (vt == LUA_TNUMBER)
                        {
                            component_json.AddMember(key_json, static_cast<float>(lua_tonumber(L, -1)), allocator);
                        }
                        else if (vt == LUA_TSTRING)
                        {
                            const char* sv = lua_tostring(L, -1);
                            rapidjson::Value sv_json;
                            sv_json.SetString(sv, allocator);
                            component_json.AddMember(key_json, sv_json, allocator);
                        }
                        else if (vt == LUA_TBOOLEAN)
                        {
                            component_json.AddMember(key_json, lua_toboolean(L, -1) != 0, allocator);
                        }
                    }
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
        else if (component_ref.isUserdata())
        {
            add_string("type");
            add_number("x");
            add_number("y");
            add_string("body_type");
            add_bool("precise");
            add_number("gravity_scale");
            add_number("density");
            add_number("angular_friction");
            add_number("rotation");
            add_string("collider_type");
            add_number("width");
            add_number("height");
            add_number("radius");
            add_string("trigger_type");
            add_number("trigger_width");
            add_number("trigger_height");
            add_number("trigger_radius");
            add_number("friction");
            add_number("bounciness");
            add_bool("has_collider");
            add_bool("has_trigger");

            add_number("emit_angle_min");
            add_number("emit_angle_max");
            add_number("emit_radius_min");
            add_number("emit_radius_max");
            add_int("frames_between_bursts");
            add_int("burst_quantity");
            add_number("start_scale_min");
            add_number("start_scale_max");
            add_number("rotation_min");
            add_number("rotation_max");
            add_number("start_speed_min");
            add_number("start_speed_max");
            add_number("rotation_speed_min");
            add_number("rotation_speed_max");
            add_int("start_color_r");
            add_int("start_color_g");
            add_int("start_color_b");
            add_int("start_color_a");
            add_int("duration_frames");
            add_number("gravity_scale_x");
            add_number("gravity_scale_y");
            add_number("drag_factor");
            add_number("angular_drag_factor");
            add_number("end_scale");
            add_int("end_color_r");
            add_int("end_color_g");
            add_int("end_color_b");
            add_int("end_color_a");
            add_string("image");
            add_int("sorting_order");
            add_bool("enabled");
        }
        else
        {
            continue;
        }

        rapidjson::Value comp_key_json;
        comp_key_json.SetString(component_key.c_str(), static_cast<rapidjson::SizeType>(component_key.size()), allocator);
        components_json.AddMember(comp_key_json, component_json, allocator);
    }

    actor_doc.AddMember("components", components_json, allocator);

    const std::filesystem::path template_path = std::filesystem::path("resources/actor_templates") / (normalized + ".template");
    FILE* file = fopen(template_path.string().c_str(), "wb");
    if (file == nullptr)
    {
        return false;
    }

    char write_buffer[65536];
    rapidjson::FileWriteStream output_stream(file, write_buffer, sizeof(write_buffer));
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(output_stream);
    actor_doc.Accept(writer);
    std::fclose(file);

    if (template_db)
    {
        template_db->LoadTemplates("resources/actor_templates");
    }
    return true;
}

void SceneEditor::InstantiateTemplateInEditor(const std::string& template_name)
{
    if (template_name.empty() || !template_db || !scene_db)
    {
        return;
    }

    if (!IsValidLuaIdentifier(template_name))
    {
        saved_message = "Invalid template name";
        saved_message_frames = 180;
        return;
    }

    const std::filesystem::path template_path = std::filesystem::path("resources/actor_templates") / (template_name + ".template");
    if (!std::filesystem::exists(template_path))
    {
        saved_message = "Template not found: " + template_name;
        saved_message_frames = 180;
        return;
    }

    std::shared_ptr<Actor> actor = template_db->Instantiate(template_name);
    if (!actor)
    {
        saved_message = "Failed to instantiate template";
        saved_message_frames = 180;
        return;
    }

    scene_db->GetActors().push_back(actor);
    selected_entity = actor;
    if (!CurrentSceneName().empty())
    {
        SaveScene(CurrentSceneName());
    }
    saved_message = "Instantiated template: " + template_name;
    saved_message_frames = 180;
}

void SceneEditor::SaveScene(const std::string& scene_name)
{
    if (scene_db == nullptr)
    {
        return;
    }

    if (is_playing)
    {
        return;
    }

    // Ensure the resources and scenes directories exist
    std::filesystem::create_directories("resources");
    std::filesystem::create_directories("resources/scenes");

    rapidjson::Document scene_doc;
    scene_doc.SetObject();
    auto& allocator = scene_doc.GetAllocator();

    // Keys that should NOT be written to the scene file (Lua lifecycle / runtime fields)
    static const std::unordered_set<std::string> kSkipSaveKeys = {
        "OnStart", "OnUpdate", "OnLateUpdate", "OnDestroy",
        "OnCollisionEnter", "OnCollisionExit", "OnTriggerEnter", "OnTriggerExit",
        "key", "actor"
    };

    rapidjson::Value actors_json(rapidjson::kArrayType);
    for (const auto& actor : scene_db->GetActors())
    {
        if (!actor) { continue; }

        rapidjson::Value actor_json(rapidjson::kObjectType);
        rapidjson::Value name_json;
        name_json.SetString(actor->GetName().c_str(), static_cast<rapidjson::SizeType>(actor->GetName().size()), allocator);
        actor_json.AddMember("name", name_json, allocator);

        rapidjson::Value components_json(rapidjson::kObjectType);
        for (const auto& [component_key, component_ref_ptr] : actor->components)
        {
            if (!component_ref_ptr) { continue; }
            luabridge::LuaRef component_ref = *component_ref_ptr;
            rapidjson::Value component_json(rapidjson::kObjectType);

            auto add_number = [&](const char* key_name)
            {
                luabridge::LuaRef ref = component_ref[key_name];
                if (!ref.isNumber()) return;
                const float value = static_cast<float>(ref.cast<float>());
                if (!std::isfinite(value)) return;
                rapidjson::Value k;
                k.SetString(key_name, allocator);
                component_json.AddMember(k, value, allocator);
            };

            auto add_int = [&](const char* key_name)
            {
                luabridge::LuaRef ref = component_ref[key_name];
                if (!ref.isNumber()) return;
                rapidjson::Value k;
                k.SetString(key_name, allocator);
                component_json.AddMember(k, ref.cast<int>(), allocator);
            };

            auto add_bool = [&](const char* key_name)
            {
                luabridge::LuaRef ref = component_ref[key_name];
                if (!ref.isBool()) return;
                rapidjson::Value k;
                k.SetString(key_name, allocator);
                component_json.AddMember(k, ref.cast<bool>(), allocator);
            };

            auto add_string = [&](const char* key_name)
            {
                luabridge::LuaRef ref = component_ref[key_name];
                if (!ref.isString()) return;
                rapidjson::Value k;
                k.SetString(key_name, allocator);
                rapidjson::Value v;
                const std::string s = ref.cast<std::string>();
                v.SetString(s.c_str(), static_cast<rapidjson::SizeType>(s.size()), allocator);
                component_json.AddMember(k, v, allocator);
            };

            // Iterate ALL table keys and save string/number/bool values
            lua_State* L = ComponentDB::GetLuaState();
            if (component_ref.isTable() && L)
            {
                component_ref.push(L);
                lua_pushnil(L);
                while (lua_next(L, -2) != 0)
                {
                    if (lua_type(L, -2) == LUA_TSTRING)
                    {
                        std::string k(lua_tostring(L, -2));
                        if (kSkipSaveKeys.find(k) == kSkipSaveKeys.end())
                        {
                            int vt = lua_type(L, -1);
                            rapidjson::Value key_json;
                            key_json.SetString(k.c_str(), static_cast<rapidjson::SizeType>(k.size()), allocator);
                            if (vt == LUA_TNUMBER)
                            {
                                component_json.AddMember(key_json, static_cast<float>(lua_tonumber(L, -1)), allocator);
                            }
                            else if (vt == LUA_TSTRING)
                            {
                                const char* sv = lua_tostring(L, -1);
                                rapidjson::Value sv_json;
                                sv_json.SetString(sv, allocator);
                                component_json.AddMember(key_json, sv_json, allocator);
                            }
                            else if (vt == LUA_TBOOLEAN)
                            {
                                component_json.AddMember(key_json, lua_toboolean(L, -1) != 0, allocator);
                            }
                        }
                    }
                    lua_pop(L, 1);
                }
                lua_pop(L, 1);
            }
            else if (component_ref.isUserdata())
            {
                add_string("type");
                add_number("x");
                add_number("y");
                add_string("body_type");
                add_bool("precise");
                add_number("gravity_scale");
                add_number("density");
                add_number("angular_friction");
                add_number("rotation");
                add_string("collider_type");
                add_number("width");
                add_number("height");
                add_number("radius");
                add_string("trigger_type");
                add_number("trigger_width");
                add_number("trigger_height");
                add_number("trigger_radius");
                add_number("friction");
                add_number("bounciness");
                add_bool("has_collider");
                add_bool("has_trigger");

                add_number("emit_angle_min");
                add_number("emit_angle_max");
                add_number("emit_radius_min");
                add_number("emit_radius_max");
                add_int("frames_between_bursts");
                add_int("burst_quantity");
                add_number("start_scale_min");
                add_number("start_scale_max");
                add_number("rotation_min");
                add_number("rotation_max");
                add_number("start_speed_min");
                add_number("start_speed_max");
                add_number("rotation_speed_min");
                add_number("rotation_speed_max");
                add_int("start_color_r");
                add_int("start_color_g");
                add_int("start_color_b");
                add_int("start_color_a");
                add_int("duration_frames");
                add_number("gravity_scale_x");
                add_number("gravity_scale_y");
                add_number("drag_factor");
                add_number("angular_drag_factor");
                add_number("end_scale");
                add_int("end_color_r");
                add_int("end_color_g");
                add_int("end_color_b");
                add_int("end_color_a");
                add_string("image");
                add_int("sorting_order");
                add_bool("enabled");
            }
            else
            {
                continue;
            }

            rapidjson::Value comp_key_json;
            comp_key_json.SetString(component_key.c_str(), static_cast<rapidjson::SizeType>(component_key.size()), allocator);
            components_json.AddMember(comp_key_json, component_json, allocator);
        }

        actor_json.AddMember("components", components_json, allocator);
        actors_json.PushBack(actor_json, allocator);
    }

    scene_doc.AddMember("actors", actors_json, allocator);

    const std::string scene_path = "resources/scenes/" + scene_name + ".scene";
    FILE* file = fopen(scene_path.c_str(), "wb");
    if (file == nullptr)
    {
        saved_message = "Failed to save scene";
        saved_message_frames = 180;
        return;
    }

    char write_buffer[65536];
    rapidjson::FileWriteStream output_stream(file, write_buffer, sizeof(write_buffer));
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(output_stream);
    scene_doc.Accept(writer);
    std::fclose(file);

    saved_message = "Saved: " + scene_name;
    saved_message_frames = 180;
}

void SceneEditor::ReloadScene(const std::string& scene_name)
{
    if (scene_db == nullptr || template_db == nullptr || component_db == nullptr)
    {
        return;
    }

    if (scene_db->LoadScene(scene_name, *template_db, *component_db))
    {
        saved_message = "Reloaded scene: " + scene_name;
        saved_message_frames = 180;
    }
    else
    {
        saved_message = "Failed to reload scene";
        saved_message_frames = 180;
    }
}
