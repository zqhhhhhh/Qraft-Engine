#include <filesystem>
#include <iostream>
#include <algorithm>
#include "renderer/AudioDB.h"

AudioDB* AudioDB::active_instance = nullptr;

AudioDB::AudioDB()
{
    active_instance = this;
}

AudioDB::~AudioDB()
{
    active_instance = nullptr;
    loaded_audio.clear();
    if (initialized)
    {
        AudioHelper::Mix_CloseAudio();
    }
}

void AudioDB::Init()
{
    if (initialized) return;
    AudioHelper::Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    AudioHelper::Mix_AllocateChannels(50);
    initialized = true;
}

Mix_Chunk* AudioDB::LoadClip(const std::string& audio_clip_name)
{
    auto it = loaded_audio.find(audio_clip_name);
    if (it != loaded_audio.end()) return it->second;

    std::string wav = "resources/audio/" + audio_clip_name + ".wav";
    std::string ogg = "resources/audio/" + audio_clip_name + ".ogg";
    std::string path = "";
    if (std::filesystem::exists(wav)) path = wav;
    else if (std::filesystem::exists(ogg)) path = ogg;
    else
    {
        std::cout << "error: failed to play audio clip " << audio_clip_name;
        exit(0);
    }

    Mix_Chunk* chunk = AudioHelper::Mix_LoadWAV(path.c_str());
    loaded_audio[audio_clip_name] = chunk;
    return chunk;
}

int AudioDB::PlayChannel(int channel, const std::string& audio_clip_name, bool does_loop)
{
    if (audio_clip_name.empty()) return -1;
    if (!initialized) Init();
    Mix_Chunk* chunk = LoadClip(audio_clip_name);
    int loops = does_loop ? -1 : 0;
    return AudioHelper::Mix_PlayChannel(channel, chunk, loops);
}

int AudioDB::HaltChannel(int channel)
{
    return AudioHelper::Mix_HaltChannel(channel);
}

/* Lua API */
void AudioDB::LuaPlayAudio(int channel, const std::string& clip_name, bool loop)
{
    if (AudioDB::active_instance == nullptr)
    {
        return;
    }
    AudioDB::active_instance->PlayChannel(channel, clip_name, loop);
}

void AudioDB::LuaHaltAudio(int channel)
{
    if (AudioDB::active_instance == nullptr)
    {
        return;
    }
    AudioDB::active_instance->HaltChannel(channel);
}

void AudioDB::LuaSetVolume(int channel, float volume)
{
    const int volume_int = static_cast<int>(volume);
    const int clamped_volume = std::max(0, std::min(128, volume_int));
    AudioHelper::Mix_Volume(channel, clamped_volume);
}