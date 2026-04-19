#include <filesystem>
#include <iostream>
#include <algorithm>
#include "engine/utils/JsonUtils.h"
#include "renderer/Renderer.h"
#include "autograder/Helper.h"

Renderer* Renderer::active_instance = nullptr;

Renderer::Renderer()
{
    active_instance = this;
}

Renderer::~Renderer() 
{
    active_instance = nullptr;
    if (audio_db) audio_db.reset();
    if (text_db) text_db.reset();
    if (image_db) image_db.reset();
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

void Renderer::Config(const rapidjson::Document& game_config)
{
    if (game_config.HasMember("game_title") && game_config["game_title"].IsString()) 
    {
        title = game_config["game_title"].GetString();
    }
    if (std::filesystem::exists("resources/rendering.config")) 
    {
        rapidjson::Document rendering_config_doc;
        JsonUtils::ReadJsonFile("resources/rendering.config", rendering_config_doc);
        if (rendering_config_doc.HasMember("x_resolution") && rendering_config_doc["x_resolution"].IsInt()) 
        {
            camera_dim.x = rendering_config_doc["x_resolution"].GetInt();
        } 
        if (rendering_config_doc.HasMember("y_resolution") && rendering_config_doc["y_resolution"].IsInt()) 
        {
            camera_dim.y = rendering_config_doc["y_resolution"].GetInt();
        }
        if (rendering_config_doc.HasMember("clear_color_r") && rendering_config_doc["clear_color_r"].IsUint()) 
        {
            clear_r = static_cast<Uint8>(rendering_config_doc["clear_color_r"].GetUint());
        }
        if (rendering_config_doc.HasMember("clear_color_g") && rendering_config_doc["clear_color_g"].IsUint()) 
        {
            clear_g = static_cast<Uint8>(rendering_config_doc["clear_color_g"].GetUint());
        }
        if (rendering_config_doc.HasMember("clear_color_b") && rendering_config_doc["clear_color_b"].IsUint()) 
        {
            clear_b = static_cast<Uint8>(rendering_config_doc["clear_color_b"].GetUint());
        }
        if (rendering_config_doc.HasMember("cam_offset_x") && rendering_config_doc["cam_offset_x"].IsNumber())
        {
            cam_offset_x = rendering_config_doc["cam_offset_x"].GetFloat();
        }
        if (rendering_config_doc.HasMember("cam_offset_y") && rendering_config_doc["cam_offset_y"].IsNumber())
        {
            cam_offset_y = rendering_config_doc["cam_offset_y"].GetFloat();
        }
        if (rendering_config_doc.HasMember("zoom_factor") && rendering_config_doc["zoom_factor"].IsNumber())
        {
            zoom_factor = rendering_config_doc["zoom_factor"].GetFloat();
        }
        if (rendering_config_doc.HasMember("cam_ease_factor") && rendering_config_doc["cam_ease_factor"].IsNumber())
        {
            cam_ease_factor = rendering_config_doc["cam_ease_factor"].GetFloat();
        }
    }
}

bool Renderer::Init(const rapidjson::Document& game_config)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) 
    {
        std::cout << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return false;
    }
    window = Helper::SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, camera_dim.x, camera_dim.y, SDL_WINDOW_SHOWN);
    renderer = Helper::SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (window == nullptr || renderer == nullptr) 
    {
        std::cout << "Failed to create window or renderer: " << SDL_GetError() << std::endl;
        return false;
    }
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    image_db = std::make_unique<ImageDB>(renderer);
    TTF_Init();
    text_db = std::make_unique<TextDB>();
    text_db->LoadFont(game_config);
    audio_db = std::make_unique<AudioDB>();
    audio_db->Init();
    return true;
}

void Renderer::Present() 
{
    Helper::SDL_RenderPresent(renderer);
}

void Renderer::Clear()
{
    SDL_SetRenderDrawColor(renderer, clear_r, clear_g, clear_b, 255);
    SDL_RenderClear(renderer);
}

/* Text Lua */
void Renderer::RenderAndClearAllTextRequests()
{
    if (text_db == nullptr)
    {
        return;
    }
    while (text_db->HasPendingTextRenderRequest())
    {
        TextRenderRequest request = text_db->DequeueTextRenderRequest();
        text_db->DrawTextRequest(renderer, request);
    }
    text_db->ClearTextRenderRequests();
}

/* Image Lua */
void Renderer::RenderAndClearAllImageRequests()
{
    if (image_db == nullptr)
    {
        return;
    }
    image_db->RenderAndClearSceneRequests(renderer, camera_dim.x, camera_dim.y, zoom_factor, cam_offset_x, cam_offset_y);
}

void Renderer::RenderAndClearAllUI()
{
    if (image_db == nullptr)
    {
        return;
    }
    image_db->RenderAndClearUIRequests(renderer);
}

void Renderer::RenderAndClearAllPixels()
{
    if (image_db == nullptr)
    {
        return;
    }
    image_db->RenderAndClearPixelRequests(renderer);
}

/* Camera Lua */
void Renderer::LuaSetCameraPosition(float x, float y)
{
    if (active_instance == nullptr)
    {
        return;
    }
    active_instance->cam_offset_x = x;
    active_instance->cam_offset_y = y;
}

float Renderer::LuaGetCameraPositionX()
{
    return active_instance == nullptr ? 0.0f : active_instance->cam_offset_x;
}

float Renderer::LuaGetCameraPositionY()
{
    return active_instance == nullptr ? 0.0f : active_instance->cam_offset_y;
}

void Renderer::LuaSetCameraZoom(float zoom)
{
    if (active_instance == nullptr)
    {
        return;
    }
    active_instance->zoom_factor = zoom;
}

float Renderer::LuaGetCameraZoom()
{
    return active_instance == nullptr ? 1.0f : active_instance->zoom_factor;
}