#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include "renderer/ImageDB.h"
#include "autograder/Helper.h"

ImageDB* ImageDB::active_instance = nullptr;

ImageDB::ImageDB(SDL_Renderer* renderer_)
{
    active_instance = this;
    renderer = renderer_;
}

ImageDB::~ImageDB()
{
    active_instance = nullptr;
    Clear();
}

SDL_Texture* ImageDB::LoadTexture(SDL_Renderer* renderer, const std::string& filename) 
{
    if (textures.count(filename)) {
        return textures[filename].texture;
    }

    std::filesystem::path requested(filename);
    std::filesystem::path image_path;

    if (requested.has_extension())
    {
        image_path = std::filesystem::path("resources/images") / requested;
        if (!std::filesystem::exists(image_path))
        {
            const std::filesystem::path fallback = std::filesystem::path("resources/images") / (requested.stem().string() + ".png");
            if (std::filesystem::exists(fallback))
            {
                image_path = fallback;
            }
        }
    }
    else
    {
        image_path = std::filesystem::path("resources/images") / (filename + ".png");
    }

    if (image_path.empty() || !std::filesystem::exists(image_path)) {
        std::cout << "error: missing image " << filename;
        exit(0);
    }

    SDL_Texture* tex = IMG_LoadTexture(renderer, image_path.string().c_str());
    if (tex == nullptr) {
        std::cout << "Failed to load image: " << image_path.string()
                << " SDL_Error: " << SDL_GetError() << std::endl;
        exit(0);
    }

    TextureCacheEntry entry;
    entry.texture = tex;
    Helper::SDL_QueryTexture(tex, &entry.width, &entry.height);
    textures[filename] = entry;
    return tex;
}

const ImageDB::TextureCacheEntry& ImageDB::GetTextureCacheEntry(SDL_Renderer* renderer, const std::string& filename)
{
    if (!textures.count(filename))
    {
        LoadTexture(renderer, filename);
    }
    return textures.at(filename);
}

void ImageDB::Clear() 
{
    for (auto& pair : textures) {
        if (pair.second.texture) SDL_DestroyTexture(pair.second.texture);
    }
    textures.clear();
    scene_image_draw_requests.clear();
    particle_draw_orders.clear();
    particle_draw_requests_by_order.clear();
    ui_image_draw_requests.clear();
    pixel_draw_requests.clear();
}

void ImageDB::EnqueueSceneImageRequest(const SceneImageDrawRequest& request)
{
    scene_image_draw_requests.push_back(request);
}

void ImageDB::EnqueueParticleRequest(const ParticleDrawRequest& request, int sorting_order)
{
    auto [it, inserted] = particle_draw_requests_by_order.try_emplace(sorting_order);
    if (inserted)
    {
        particle_draw_orders.push_back(sorting_order);
    }
    it->second.push_back(request);
}

void ImageDB::EnqueueUIImageRequest(const UIImageDrawRequest& request)
{
    ui_image_draw_requests.push_back(request);
}

void ImageDB::EnqueuePixelRequest(const PixelDrawRequest& request)
{
    pixel_draw_requests.push_back(request);
}

void ImageDB::RenderAndClearSceneRequests(SDL_Renderer* renderer_ptr, int screen_width, int screen_height, float zoom_factor, float camera_x, float camera_y)
{
    std::stable_sort(scene_image_draw_requests.begin(), scene_image_draw_requests.end(),
        [](const SceneImageDrawRequest& a, const SceneImageDrawRequest& b) {
            return a.sorting_order < b.sorting_order;
        });

    std::sort(particle_draw_orders.begin(), particle_draw_orders.end());

    SDL_RenderSetScale(renderer_ptr, zoom_factor, zoom_factor);

    const float pixels_per_meter = 100.0f;
    const float inv_zoom = 1.0f / zoom_factor;

    auto render_scene_request = [&](const SceneImageDrawRequest& request)
    {
        const auto& texture_info = GetTextureCacheEntry(renderer_ptr, request.image_name);
        SDL_Texture* tex = texture_info.texture;
        const float tex_w = texture_info.width;
        const float tex_h = texture_info.height;

        /* Apply scale */
        int flip_mode = SDL_FLIP_NONE;
        if (request.scale_x < 0) flip_mode |= SDL_FLIP_HORIZONTAL;
        if (request.scale_y < 0) flip_mode |= SDL_FLIP_VERTICAL;

        float final_w = tex_w * glm::abs(request.scale_x);
        float final_h = tex_h * glm::abs(request.scale_y);

        SDL_FPoint pivot_point = { request.pivot_x * final_w, request.pivot_y * final_h };

        float screen_x = (request.x - camera_x) * pixels_per_meter
                         + screen_width  * 0.5f * (1.0f / zoom_factor) - pivot_point.x;
        float screen_y = (request.y - camera_y) * pixels_per_meter
                         + screen_height * 0.5f * (1.0f / zoom_factor) - pivot_point.y;

        SDL_FRect dst{ screen_x, screen_y, final_w, final_h };

        /* Apply tint / alpha to texture */
        SDL_SetTextureColorMod(tex, static_cast<Uint8>(request.r), static_cast<Uint8>(request.g), static_cast<Uint8>(request.b));
        SDL_SetTextureAlphaMod(tex, static_cast<Uint8>(request.a));

        /* Perform Draw */
        Helper::SDL_RenderCopyEx(0, "", renderer_ptr, tex, nullptr, &dst,
                                 static_cast<float>(request.rotation_degrees),
                                 &pivot_point, static_cast<SDL_RendererFlip>(flip_mode));

        /* Remove tint / alpha from texture */
        SDL_SetTextureColorMod(tex, 255, 255, 255);
        SDL_SetTextureAlphaMod(tex, 255);
    };

    struct ParticleTextureCacheItem
    {
        const TextureCacheEntry* texture_info = nullptr;
    };
    std::unordered_map<const std::string*, ParticleTextureCacheItem> particle_texture_cache;
    particle_texture_cache.reserve(16);

    auto render_particle_request = [&](const ParticleDrawRequest& request)
    {
        if (request.image_name == nullptr)
        {
            return;
        }

        auto cache_it = particle_texture_cache.find(request.image_name);
        if (cache_it == particle_texture_cache.end())
        {
            ParticleTextureCacheItem item;
            item.texture_info = &GetTextureCacheEntry(renderer_ptr, *request.image_name);
            cache_it = particle_texture_cache.emplace(request.image_name, item).first;
        }

        SDL_Texture* tex = cache_it->second.texture_info->texture;
        const float tex_w = cache_it->second.texture_info->width;
        const float tex_h = cache_it->second.texture_info->height;

        float final_w = tex_w * request.scale_x;
        float final_h = tex_h * request.scale_y;
        SDL_FPoint pivot_point = { 0.5f * final_w, 0.5f * final_h };

        float screen_x = (request.x - camera_x) * pixels_per_meter
                         + screen_width  * 0.5f * inv_zoom - pivot_point.x;
        float screen_y = (request.y - camera_y) * pixels_per_meter
                         + screen_height * 0.5f * inv_zoom - pivot_point.y;

        SDL_FRect dst{ screen_x, screen_y, final_w, final_h };

        SDL_SetTextureColorMod(tex, static_cast<Uint8>(request.r), static_cast<Uint8>(request.g), static_cast<Uint8>(request.b));
        SDL_SetTextureAlphaMod(tex, static_cast<Uint8>(request.a));

        Helper::SDL_RenderCopyEx(0, "", renderer_ptr, tex, nullptr, &dst,
                                 static_cast<float>(request.rotation_degrees),
                                 &pivot_point, SDL_FLIP_NONE);

        SDL_SetTextureColorMod(tex, 255, 255, 255);
        SDL_SetTextureAlphaMod(tex, 255);
    };

    size_t scene_idx = 0;
    size_t particle_order_idx = 0;
    while (scene_idx < scene_image_draw_requests.size() || particle_order_idx < particle_draw_orders.size())
    {
        const bool should_draw_scene =
            (particle_order_idx >= particle_draw_orders.size()) ||
            (scene_idx < scene_image_draw_requests.size() &&
             scene_image_draw_requests[scene_idx].sorting_order <= particle_draw_orders[particle_order_idx]);

        if (should_draw_scene)
        {
            render_scene_request(scene_image_draw_requests[scene_idx]);
            ++scene_idx;
            continue;
        }

        const int order = particle_draw_orders[particle_order_idx];
        auto bucket_it = particle_draw_requests_by_order.find(order);
        if (bucket_it != particle_draw_requests_by_order.end())
        {
            const auto& bucket = bucket_it->second;
            for (const auto& request : bucket)
            {
                render_particle_request(request);
            }
        }
        ++particle_order_idx;
    }

    SDL_RenderSetScale(renderer_ptr, 1.0f, 1.0f);
    scene_image_draw_requests.clear();
    particle_draw_orders.clear();
    particle_draw_requests_by_order.clear();
}

void ImageDB::RenderAndClearUIRequests(SDL_Renderer* renderer_ptr)
{
    std::sort(ui_image_draw_requests.begin(), ui_image_draw_requests.end(),
        [](const UIImageDrawRequest& a, const UIImageDrawRequest& b) {
            return a.sorting_order < b.sorting_order;
        });
    for (const auto& request : ui_image_draw_requests)
    {
        const auto& texture_info = GetTextureCacheEntry(renderer_ptr, request.image_name);
        SDL_Texture* tex = texture_info.texture;
        const float tex_w = texture_info.width;
        const float tex_h = texture_info.height;

        SDL_FRect dst{
            static_cast<float>(request.x),
            static_cast<float>(request.y),
            tex_w,
            tex_h
        };

        SDL_SetTextureColorMod(tex, static_cast<Uint8>(request.r), static_cast<Uint8>(request.g), static_cast<Uint8>(request.b));
        SDL_SetTextureAlphaMod(tex, static_cast<Uint8>(request.a));

        Helper::SDL_RenderCopy(renderer_ptr, tex, nullptr, &dst);

        SDL_SetTextureColorMod(tex, 255, 255, 255);
        SDL_SetTextureAlphaMod(tex, 255);
    }
    ui_image_draw_requests.clear();
}

void ImageDB::RenderAndClearPixelRequests(SDL_Renderer* renderer_ptr)
{
    SDL_SetRenderDrawBlendMode(renderer_ptr, SDL_BLENDMODE_BLEND);
    for (const auto& request : pixel_draw_requests)
    {
        SDL_SetRenderDrawColor(renderer_ptr,
            static_cast<Uint8>(request.r),
            static_cast<Uint8>(request.g),
            static_cast<Uint8>(request.b),
            static_cast<Uint8>(request.a));
        SDL_RenderDrawPoint(renderer_ptr, request.x, request.y);
    }
    SDL_SetRenderDrawBlendMode(renderer_ptr, SDL_BLENDMODE_NONE);
    pixel_draw_requests.clear();
}

void ImageDB::LuaDrawUI(const std::string& image_name, float x, float y)
{
    if (active_instance == nullptr)
    {
        return;
    }
    UIImageDrawRequest request;
    request.image_name = image_name;
    request.x = static_cast<int>(x);
    request.y = static_cast<int>(y);
    active_instance->EnqueueUIImageRequest(request);
}

void ImageDB::LuaDrawUIEx(const std::string& image_name, float x, float y, float r, float g, float b, float a, float sorting_order)
{
    if (active_instance == nullptr)
    {
        return;
    }
    UIImageDrawRequest request;
    request.image_name = image_name;
    request.x = static_cast<int>(x);
    request.y = static_cast<int>(y);
    request.r = static_cast<int>(r);
    request.g = static_cast<int>(g);
    request.b = static_cast<int>(b);
    request.a = static_cast<int>(a);
    request.sorting_order = static_cast<int>(sorting_order);
    active_instance->EnqueueUIImageRequest(request);
}

void ImageDB::LuaDraw(const std::string& image_name, float x, float y)
{
    if (active_instance == nullptr)
    {
        return;
    }
    SceneImageDrawRequest request;
    request.image_name = image_name;
    request.x = x;
    request.y = y;
    request.rotation_degrees = 0;
    request.scale_x = 1.0f;
    request.scale_y = 1.0f;
    request.pivot_x = 0.5f;
    request.pivot_y = 0.5f;
    request.r = 255;
    request.g = 255;
    request.b = 255;
    request.a = 255;
    request.sorting_order = 0;
    active_instance->EnqueueSceneImageRequest(request);
}

void ImageDB::LuaDrawEx(const std::string& image_name, float x, float y, float rotation_degrees, float scale_x, float scale_y, float pivot_x, float pivot_y, float r, float g, float b, float a, float sorting_order)
{
    if (active_instance == nullptr)
    {
        return;
    }
    SceneImageDrawRequest request;
    request.image_name = image_name;
    request.x = x;
    request.y = y;
    request.rotation_degrees = static_cast<int>(rotation_degrees);
    request.scale_x = scale_x;
    request.scale_y = scale_y;
    request.pivot_x = pivot_x;
    request.pivot_y = pivot_y;
    request.r = static_cast<int>(r);
    request.g = static_cast<int>(g);
    request.b = static_cast<int>(b);
    request.a = static_cast<int>(a);
    request.sorting_order = static_cast<int>(sorting_order);
    active_instance->EnqueueSceneImageRequest(request);
}

void ImageDB::LuaDrawPixel(float x, float y, float r, float g, float b, float a)
{
    if (active_instance == nullptr)
    {
        return;
    }
    PixelDrawRequest request;
    request.x = static_cast<int>(x);
    request.y = static_cast<int>(y);
    request.r = static_cast<int>(r);
    request.g = static_cast<int>(g);
    request.b = static_cast<int>(b);
    request.a = static_cast<int>(a);
    active_instance->EnqueuePixelRequest(request);
}

void ImageDB::CreateDefaultParticleTextureWithName(const std::string& name)
{
    if (active_instance == nullptr) return;
    if (textures.find(name) != textures.end()) return;
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 8, 8, 32, SDL_PIXELFORMAT_RGBA8888);
    Uint32 white_color = SDL_MapRGBA(surface->format, 255, 255, 255, 255);
    SDL_FillRect(surface, nullptr, white_color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(active_instance->renderer, surface);
    SDL_FreeSurface(surface);

    TextureCacheEntry entry;
    entry.texture = texture;
    entry.width = 8.0f;
    entry.height = 8.0f;
    textures[name] = entry;
}