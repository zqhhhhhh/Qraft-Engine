#include <iostream>
#include <filesystem>
#include "renderer/TextDB.h"

TextDB* TextDB::active_instance = nullptr;

TextDB::TextDB()
{
    active_instance = this;
}

TextDB::~TextDB()
{
    active_instance = nullptr;
    for (auto& [font_name, sized_fonts] : fonts)
    {
        (void)font_name;
        for (auto& [font_size, font] : sized_fonts)
        {
            (void)font_size;
            if (font)
            {
                TTF_CloseFont(font);
            }
        }
    }
    fonts.clear();
    while (!text_draw_request_queue.empty())
    {
        text_draw_request_queue.pop();
    }
}

bool TextDB::LoadFont(const rapidjson::Document &game_config)
{
    if (!game_config.HasMember("font"))
    {
        return false;
    }
    default_font_name = game_config["font"].GetString();
    return GetFont(default_font_name, 16) != nullptr;
}

TTF_Font* TextDB::GetFont(const std::string& font_name, int font_size)
{
    // Check if font already loaded
    auto by_name = fonts.find(font_name);
    if (by_name != fonts.end())
    {
        auto by_size = by_name->second.find(font_size);
        if (by_size != by_name->second.end())
        {
            return by_size->second;
        }
    }
    // Load font from file
    std::string font_path = "resources/fonts/" + font_name + ".ttf";
    if (!std::filesystem::exists(font_path))
    {
        std::cout << "error: font " << font_name << " missing";
        exit(0);
    }
    TTF_Font* font = TTF_OpenFont(font_path.c_str(), font_size);
    if (font == nullptr)
    {
        std::cout << "error: font " << font_name << " failed to load";
        exit(0);
    }
    fonts[font_name][font_size] = font;
    return font;
}

void TextDB::DrawTextRequest(SDL_Renderer* renderer, const TextRenderRequest& request)
{
    if (renderer == nullptr)
    {
        return;
    }
    TTF_Font* font = GetFont(request.font_name, request.font_size);
    if (font == nullptr)
    {
        return;
    }
    SDL_Surface* surf = TTF_RenderText_Solid(font, request.content.c_str(), request.color);
    if (surf == nullptr)
    {
        return;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex == nullptr)
    {
        SDL_FreeSurface(surf);
        return;
    }
    SDL_FRect dst{
        static_cast<float>(request.x),
        static_cast<float>(request.y),
        static_cast<float>(surf->w),
        static_cast<float>(surf->h)
    };
    SDL_RenderCopyF(renderer, tex, nullptr, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

void TextDB::EnqueueTextRenderRequest(const TextRenderRequest& request)
{
    text_draw_request_queue.push(request);
}

bool TextDB::HasPendingTextRenderRequest() const
{
    return !text_draw_request_queue.empty();
}

TextRenderRequest TextDB::DequeueTextRenderRequest()
{
    TextRenderRequest request;
    if (text_draw_request_queue.empty())
    {
        return request;
    }
    request = text_draw_request_queue.front();
    text_draw_request_queue.pop();
    return request;
}

void TextDB::ClearTextRenderRequests()
{
    while (!text_draw_request_queue.empty())
    {
        text_draw_request_queue.pop();
    }
}

/* Lua API */
void TextDB::LuaDrawText(const std::string& content, float x, float y, const std::string& font_name, float font_size, float r, float g, float b, float a)
{
    if (active_instance == nullptr)
    {
        return;
    }
    TextRenderRequest request;
    request.content = content;
    request.x = static_cast<int>(x);
    request.y = static_cast<int>(y);
    request.font_name = font_name;
    request.font_size = static_cast<int>(font_size);
    request.color = SDL_Color{
        static_cast<Uint8>(static_cast<int>(r)),
        static_cast<Uint8>(static_cast<int>(g)),
        static_cast<Uint8>(static_cast<int>(b)),
        static_cast<Uint8>(static_cast<int>(a))
    };
    active_instance->EnqueueTextRenderRequest(request);
}
