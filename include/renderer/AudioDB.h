#pragma once

#include <string>
#include <unordered_map>
#include "autograder/AudioHelper.h"

#ifdef __APPLE__
#include <SDL.h>
#include <SDL_mixer.h>
#elif defined(__linux__)
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <SDL2/SDL.h>
#include <SDL2_mixer/SDL_mixer.h>
#endif

class AudioDB {
public:
    static AudioDB* active_instance;
    AudioDB();
    ~AudioDB();
    void Init();
    /* Lua API */
    static void LuaPlayAudio(int channel, const std::string& clip_name, bool loop);
    static void LuaHaltAudio(int channel);
    static void LuaSetVolume(int channel, float volume);

private:
    bool initialized = false;
    std::unordered_map<std::string, Mix_Chunk*> loaded_audio;

    Mix_Chunk* LoadClip(const std::string& audio_clip_name);
    int PlayChannel(int channel, const std::string& audio_clip_name, bool does_loop);
    int HaltChannel(int channel);
};