#pragma once

#ifdef __APPLE__
#include <SDL.h>
#elif defined(__linux__)
#include <SDL2/SDL.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <SDL2/SDL.h>
#endif

#include <memory>
#include <string>
#include <vector>
#include "glm/glm.hpp"
#include "renderer/ImageDB.h"
#include "renderer/TextDB.h"
#include "renderer/AudioDB.h"
#include "engine/Actor.h"
#include "rapidjson/document.h"

class Renderer {
public:
    static Renderer* active_instance;
    Renderer();
    ~Renderer();

    void Config(const rapidjson::Document& game_config);
    bool Init(const rapidjson::Document& game_config); // create window + renderer + clear
    void Present(); // render to screen
    void Clear(); // clear
    SDL_Window* GetSDLWindow() const { return window; }
    SDL_Renderer* GetSDLRenderer() const { return renderer; }

    /* Text Lua */
    void RenderAndClearAllTextRequests();

    /* Image Lua */
    void RenderAndClearAllImageRequests();
    void RenderAndClearAllUI();
    void RenderAndClearAllPixels();

    /* Camera Lua */
    static void LuaSetCameraPosition(float x, float y);
    static float LuaGetCameraPositionX();
    static float LuaGetCameraPositionY();
    static void LuaSetCameraZoom(float zoom);
    static float LuaGetCameraZoom();
    
private:
    std::string title = "";
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    std::unique_ptr<ImageDB> image_db = nullptr;
    std::unique_ptr<TextDB> text_db = nullptr;
    std::unique_ptr<AudioDB> audio_db = nullptr;

    glm::ivec2 camera_dim{640, 360};
    Uint8 clear_r = 255;
    Uint8 clear_g = 255;
    Uint8 clear_b = 255;
    float cam_offset_x = 0.0f;
    float cam_offset_y = 0.0f;
    float zoom_factor = 1.0f;
    float cam_ease_factor = 1.0f;  
};
