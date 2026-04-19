#pragma once

#include <queue>
#include <string>
#include <unordered_map>
#include "rapidjson/document.h"

#ifdef __APPLE__
#include <SDL.h>
#include <SDL_ttf.h>
#elif defined(__linux__)
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <SDL2/SDL.h>
#include <SDL2_ttf/SDL_ttf.h>
#endif

struct TextRenderRequest
{
    std::string content;
    int x = 0;
    int y = 0;
    std::string font_name;
    int font_size = 16;
    SDL_Color color{255, 255, 255, 255};
};

class TextDB {
public:
    static TextDB* active_instance;
    TextDB();
    ~TextDB(); 
    bool LoadFont(const rapidjson::Document &game_config);
    void EnqueueTextRenderRequest(const TextRenderRequest& request);
    bool HasPendingTextRenderRequest() const;
    TextRenderRequest DequeueTextRenderRequest();
    void ClearTextRenderRequests();
    void DrawTextRequest(SDL_Renderer* renderer, const TextRenderRequest& request);
    /* Lua API */
    static void LuaDrawText(const std::string& content, float x, float y, const std::string& font_name, float font_size, float r, float g, float b, float a);

private:
    std::string default_font_name;
    std::unordered_map<std::string, std::unordered_map<int, TTF_Font*>> fonts;
    std::queue<TextRenderRequest> text_draw_request_queue;

    TTF_Font* GetFont(const std::string& font_name, int font_size);
    const std::string& GetDefaultFontName() const { return default_font_name; }
};