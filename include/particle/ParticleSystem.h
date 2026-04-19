#pragma once

#include <cstddef>
#include <deque>
#include <limits>
#include <queue>
#include <string>
#include <vector>

#ifdef __APPLE__
#include <SDL.h>
#elif defined(__linux__)
#include <SDL2/SDL.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <SDL2/SDL.h>
#endif

#include "lua/lua.hpp"
#include "LuaBridge/LuaBridge.h"
#include "autograder/Helper.h"

class Actor;

class ParticleSystem
{
public:
    ParticleSystem() = default;

    // Component identity (set by the engine)
    std::string key;
    std::string type = "ParticleSystem";
    bool enabled = true;
    Actor* actor = nullptr;

    // Emission center (scene coordinates)
    float x = 0.0f;
    float y = 0.0f;

    // Emission shape (polar coordinates)
    float emit_angle_min  = 0.0f;
    float emit_angle_max  = 360.0f;
    float emit_radius_min = 0.0f;
    float emit_radius_max = 0.5f;

    // Burst timing
    int frames_between_bursts = 1;
    int burst_quantity        = 1;

    // Per-particle appearance at spawn
    float start_scale_min = 1.0f;
    float start_scale_max = 1.0f;
    float rotation_min    = 0.0f;
    float rotation_max    = 0.0f;
    float start_speed_min = 0.0f;
    float start_speed_max = 0.0f;
    float rotation_speed_min = 0.0f;
    float rotation_speed_max = 0.0f;
    int   start_color_r   = 255;
    int   start_color_g   = 255;
    int   start_color_b   = 255;
    int   start_color_a   = 255;

    // Per-particle evolution over life
    int   duration_frames = 300;
    float gravity_scale_x = 0.0f;
    float gravity_scale_y = 0.0f;
    float drag_factor = 1.0f;
    float angular_drag_factor = 1.0f;
    float end_scale = std::numeric_limits<float>::quiet_NaN();
    int   end_color_r = -1;
    int   end_color_g = -1;
    int   end_color_b = -1;
    int   end_color_a = -1;

    // Rendering
    std::string image        = "";
    int sorting_order        = 9999;

    // Lifecycle methods
    void OnStart();
    void OnUpdate();
    void OnDestroy();
    void Stop();
    void Play();
    void Burst();

    // Accessor helpers for LuaBridge (Actor* needs explicit getter/setter)
    Actor* GetActor() const { return actor; }
    void   SetActor(Actor* a) { actor = a; }

    // ---- Static engine interface ----
    static ParticleSystem* NewInstance();
    static ParticleSystem* CloneInstance(const ParticleSystem* parent);
    static void RegisterWithLua(lua_State* lua_state);
    static void Shutdown();

private:
    void EmitBurstNow(int quantity);

    std::deque<unsigned char> active;
    std::deque<int> age_frames;
    std::deque<float> pos_x;
    std::deque<float> pos_y;
    std::deque<float> vel_x;
    std::deque<float> vel_y;
    std::deque<float> scale;
    std::deque<float> initial_scale;
    std::deque<float> rotation;
    std::deque<float> angular_speed;
    std::deque<int> color_r;
    std::deque<int> color_g;
    std::deque<int> color_b;
    std::deque<int> color_a;
    std::deque<int> initial_color_r;
    std::deque<int> initial_color_g;
    std::deque<int> initial_color_b;
    std::deque<int> initial_color_a;

    RandomEngine angle_engine;
    RandomEngine radius_engine;
    RandomEngine scale_engine;
    RandomEngine rotation_engine;
    RandomEngine speed_engine;
    RandomEngine rotation_speed_engine;
    std::queue<size_t> free_list;
    int local_frame_number = 0;
    bool emission_allowed = true;

    inline static std::vector<ParticleSystem*> all_instances;
    inline static const std::string PARTICLE_TEXTURE_NAME = "__particle_default";
};
