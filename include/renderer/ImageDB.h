#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include "glm/glm.hpp"
#include "rapidjson/document.h"

#ifdef __APPLE__
#include <SDL.h>
#include <SDL_image.h>
#elif defined(__linux__)
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <SDL2/SDL.h>
#include <SDL2_image/SDL_image.h>
#endif

struct SceneImageDrawRequest
{
    std::string image_name;
    float x = 0.0f;
    float y = 0.0f;
    int rotation_degrees = 0;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float pivot_x;
    float pivot_y;
    int r;
    int g;
    int b;
    int a;
    int sorting_order;
};

struct UIImageDrawRequest
{
    std::string image_name;
    int x = 0;
    int y = 0;
    int r = 255;
    int g = 255;
    int b = 255;
    int a = 255;
    int sorting_order = 0;
};

struct PixelDrawRequest
{
    int x = 0;
    int y = 0;
    int r = 255;
    int g = 255;
    int b = 255;
    int a = 255;
};

struct ParticleDrawRequest
{
    const std::string* image_name = nullptr;
    float x = 0.0f;
    float y = 0.0f;
    int rotation_degrees = 0;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    int r = 255;
    int g = 255;
    int b = 255;
    int a = 255;
};

class ImageDB {
public:
    static ImageDB* active_instance;
    ImageDB(SDL_Renderer* renderer_);
    ~ImageDB();

    void EnqueueSceneImageRequest(const SceneImageDrawRequest& request);
    void EnqueueParticleRequest(const ParticleDrawRequest& request, int sorting_order);
    void EnqueueUIImageRequest(const UIImageDrawRequest& request);
    void EnqueuePixelRequest(const PixelDrawRequest& request);
    void RenderAndClearSceneRequests(SDL_Renderer* renderer, int screen_width, int screen_height, float zoom_factor, float camera_x, float camera_y);
    void RenderAndClearUIRequests(SDL_Renderer* renderer);
    void RenderAndClearPixelRequests(SDL_Renderer* renderer);

    /* Lua API */
    static void LuaDrawUI(const std::string& image_name, float x, float y);
    static void LuaDrawUIEx(const std::string& image_name, float x, float y, float r, float g, float b, float a, float sorting_order);
    static void LuaDraw(const std::string& image_name, float x, float y);
    static void LuaDrawEx(const std::string& image_name, float x, float y, float rotation_degrees, float scale_x, float scale_y, float pivot_x, float pivot_y, float r, float g, float b, float a, float sorting_order);
    static void LuaDrawPixel(float x, float y, float r, float g, float b, float a);

    /* Particle Systems */
    void CreateDefaultParticleTextureWithName(const std::string& name);

private:
    struct TextureCacheEntry
    {
        SDL_Texture* texture = nullptr;
        float width = 0.0f;
        float height = 0.0f;
    };

    friend class ParticleSystem;
    SDL_Renderer* renderer;
    std::unordered_map<std::string, TextureCacheEntry> textures;
    std::vector<SceneImageDrawRequest> scene_image_draw_requests;
    std::vector<int> particle_draw_orders;
    std::unordered_map<int, std::vector<ParticleDrawRequest>> particle_draw_requests_by_order;
    std::vector<UIImageDrawRequest> ui_image_draw_requests;
    std::vector<PixelDrawRequest> pixel_draw_requests;

    SDL_Texture* LoadTexture(SDL_Renderer* renderer, const std::string& filename);
    const TextureCacheEntry& GetTextureCacheEntry(SDL_Renderer* renderer, const std::string& filename);
    void Clear();
};
