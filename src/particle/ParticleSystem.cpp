#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "particle/ParticleSystem.h"
#include "engine/Actor.h"
#include "engine/ComponentDB.h"
#include "renderer/ImageDB.h"
#include "box2d/box2d.h"
#include "glm/glm.hpp"

// Seeds for RandomEngine
static constexpr int kAngleSeed = 298;
static constexpr int kRadiusSeed = 404;
static constexpr int kScaleSeed = 494;
static constexpr int kRotationSeed = 440;
static constexpr int kSpeedSeed = 498;
static constexpr int kRotationSpeedSeed = 305;

namespace
{
    int clamp_color_255(int value)
    {
        return std::clamp(value, 0, 255);
    }
}

// ---- Static engine interface ----

ParticleSystem* ParticleSystem::NewInstance()
{
    ParticleSystem* ps = new ParticleSystem();
    all_instances.push_back(ps);
    return ps;
}

ParticleSystem* ParticleSystem::CloneInstance(const ParticleSystem* parent)
{
    ParticleSystem* ps = nullptr;
    if (parent != nullptr)
    {
        ps = new ParticleSystem(*parent);
        ps->active.clear();
        ps->age_frames.clear();
        ps->pos_x.clear();
        ps->pos_y.clear();
        ps->vel_x.clear();
        ps->vel_y.clear();
        ps->scale.clear();
        ps->initial_scale.clear();
        ps->rotation.clear();
        ps->angular_speed.clear();
        ps->color_r.clear();
        ps->color_g.clear();
        ps->color_b.clear();
        ps->color_a.clear();
        ps->initial_color_r.clear();
        ps->initial_color_g.clear();
        ps->initial_color_b.clear();
        ps->initial_color_a.clear();
        ps->angle_engine = RandomEngine();
        ps->radius_engine = RandomEngine();
        ps->scale_engine = RandomEngine();
        ps->rotation_engine = RandomEngine();
        ps->speed_engine = RandomEngine();
        ps->rotation_speed_engine = RandomEngine();
        ps->local_frame_number = 0;
        while (!ps->free_list.empty())
        {
            ps->free_list.pop();
        }
    }
    else
    {
        ps = new ParticleSystem();
    }
    all_instances.push_back(ps);
    return ps;
}

void ParticleSystem::RegisterWithLua(lua_State* lua_state)
{
    if (lua_state == nullptr)
    {
        return;
    }

    luabridge::getGlobalNamespace(lua_state)
        .beginClass<ParticleSystem>("ParticleSystem")
        .addConstructor<void (*)(void)>()
        .addProperty("key", &ParticleSystem::key)
        .addProperty("type", &ParticleSystem::type)
        .addProperty("enabled", &ParticleSystem::enabled)
        .addProperty("actor", &ParticleSystem::GetActor, &ParticleSystem::SetActor)
        .addProperty("x", &ParticleSystem::x)
        .addProperty("y", &ParticleSystem::y)
        .addProperty("emit_angle_min", &ParticleSystem::emit_angle_min)
        .addProperty("emit_angle_max", &ParticleSystem::emit_angle_max)
        .addProperty("emit_radius_min", &ParticleSystem::emit_radius_min)
        .addProperty("emit_radius_max", &ParticleSystem::emit_radius_max)
        .addProperty("frames_between_bursts", &ParticleSystem::frames_between_bursts)
        .addProperty("burst_quantity", &ParticleSystem::burst_quantity)
        .addProperty("start_scale_min", &ParticleSystem::start_scale_min)
        .addProperty("start_scale_max", &ParticleSystem::start_scale_max)
        .addProperty("rotation_min", &ParticleSystem::rotation_min)
        .addProperty("rotation_max", &ParticleSystem::rotation_max)
        .addProperty("start_speed_min", &ParticleSystem::start_speed_min)
        .addProperty("start_speed_max", &ParticleSystem::start_speed_max)
        .addProperty("rotation_speed_min", &ParticleSystem::rotation_speed_min)
        .addProperty("rotation_speed_max", &ParticleSystem::rotation_speed_max)
        .addProperty("start_color_r", &ParticleSystem::start_color_r)
        .addProperty("start_color_g", &ParticleSystem::start_color_g)
        .addProperty("start_color_b", &ParticleSystem::start_color_b)
        .addProperty("start_color_a", &ParticleSystem::start_color_a)
        .addProperty("duration_frames", &ParticleSystem::duration_frames)
        .addProperty("gravity_scale_x", &ParticleSystem::gravity_scale_x)
        .addProperty("gravity_scale_y", &ParticleSystem::gravity_scale_y)
        .addProperty("drag_factor", &ParticleSystem::drag_factor)
        .addProperty("angular_drag_factor", &ParticleSystem::angular_drag_factor)
        .addProperty("end_scale", &ParticleSystem::end_scale)
        .addProperty("end_color_r", &ParticleSystem::end_color_r)
        .addProperty("end_color_g", &ParticleSystem::end_color_g)
        .addProperty("end_color_b", &ParticleSystem::end_color_b)
        .addProperty("end_color_a", &ParticleSystem::end_color_a)
        .addProperty("image", &ParticleSystem::image)
        .addProperty("sorting_order", &ParticleSystem::sorting_order)
        .addFunction("OnStart", &ParticleSystem::OnStart)
        .addFunction("OnUpdate", &ParticleSystem::OnUpdate)
        .addFunction("OnDestroy", &ParticleSystem::OnDestroy)
        .addFunction("Stop", &ParticleSystem::Stop)
        .addFunction("Play", &ParticleSystem::Play)
        .addFunction("Burst", &ParticleSystem::Burst)
        .endClass();
}

void ParticleSystem::Shutdown()
{
    for (ParticleSystem* ps : all_instances)
    {
        delete ps;
    }
    all_instances.clear();
}

// ---- Lifecycle methods ----

void ParticleSystem::OnStart()
{
    if (ImageDB::active_instance != nullptr)
    {
        ImageDB::active_instance->CreateDefaultParticleTextureWithName(PARTICLE_TEXTURE_NAME);
    }

    active.clear();
    age_frames.clear();
    pos_x.clear();
    pos_y.clear();
    vel_x.clear();
    vel_y.clear();
    scale.clear();
    initial_scale.clear();
    rotation.clear();
    angular_speed.clear();
    color_r.clear();
    color_g.clear();
    color_b.clear();
    color_a.clear();
    initial_color_r.clear();
    initial_color_g.clear();
    initial_color_b.clear();
    initial_color_a.clear();
    while (!free_list.empty())
    {
        free_list.pop();
    }
    local_frame_number = 0;

    angle_engine.Configure(emit_angle_min, emit_angle_max, kAngleSeed);
    radius_engine.Configure(emit_radius_min, emit_radius_max, kRadiusSeed);
    scale_engine.Configure(start_scale_min, start_scale_max, kScaleSeed);
    rotation_engine.Configure(rotation_min, rotation_max, kRotationSeed);
    speed_engine.Configure(start_speed_min, start_speed_max, kSpeedSeed);
    rotation_speed_engine.Configure(rotation_speed_min, rotation_speed_max, kRotationSpeedSeed);
}

void ParticleSystem::OnUpdate()
{
    if (local_frame_number % frames_between_bursts == 0 && emission_allowed)
    {
        EmitBurstNow(burst_quantity);
    }

    const bool has_end_scale = !std::isnan(end_scale);
    const bool has_end_r = (end_color_r >= 0 && end_color_r <= 255);
    const bool has_end_g = (end_color_g >= 0 && end_color_g <= 255);
    const bool has_end_b = (end_color_b >= 0 && end_color_b <= 255);
    const bool has_end_a = (end_color_a >= 0 && end_color_a <= 255);

    const size_t n = active.size();
    for (size_t idx = 0; idx < n; ++idx)
    {
        if (!active[idx])
        {
            continue;
        }
        if (age_frames[idx] >= duration_frames)
        {
            active[idx] = 0;
            free_list.push(idx);
            continue;
        }

        vel_x[idx] += gravity_scale_x;
        vel_y[idx] += gravity_scale_y;
        vel_x[idx] *= drag_factor;
        vel_y[idx] *= drag_factor;
        angular_speed[idx] *= angular_drag_factor;
        pos_x[idx] += vel_x[idx];
        pos_y[idx] += vel_y[idx];
        rotation[idx] += angular_speed[idx];

        const float life_t = static_cast<float>(age_frames[idx]) / static_cast<float>(duration_frames);

        if (has_end_scale)
        {
            scale[idx] = glm::mix(initial_scale[idx], end_scale, life_t);
        }

        if (has_end_r)
        {
            color_r[idx] = clamp_color_255(static_cast<int>(glm::mix(static_cast<float>(initial_color_r[idx]), static_cast<float>(end_color_r), life_t)));
        }
        if (has_end_g)
        {
            color_g[idx] = clamp_color_255(static_cast<int>(glm::mix(static_cast<float>(initial_color_g[idx]), static_cast<float>(end_color_g), life_t)));
        }
        if (has_end_b)
        {
            color_b[idx] = clamp_color_255(static_cast<int>(glm::mix(static_cast<float>(initial_color_b[idx]), static_cast<float>(end_color_b), life_t)));
        }
        if (has_end_a)
        {
            color_a[idx] = clamp_color_255(static_cast<int>(glm::mix(static_cast<float>(initial_color_a[idx]), static_cast<float>(end_color_a), life_t)));
        }

        ++age_frames[idx];
    }

    if (ImageDB::active_instance != nullptr)
    {
        const std::string& tex_name = image.empty() ? PARTICLE_TEXTURE_NAME : image;

        for (size_t idx = 0; idx < n; ++idx)
        {
            if (!active[idx])
            {
                continue;
            }

            ParticleDrawRequest request;
            request.image_name = &tex_name;
            request.x = pos_x[idx];
            request.y = pos_y[idx];
            request.rotation_degrees = static_cast<int>(rotation[idx]);
            request.scale_x = scale[idx];
            request.scale_y = scale[idx];
            request.r = color_r[idx];
            request.g = color_g[idx];
            request.b = color_b[idx];
            request.a = color_a[idx];
            ImageDB::active_instance->EnqueueParticleRequest(request, sorting_order);
        }
    }

    ++local_frame_number;
}

void ParticleSystem::OnDestroy()
{
    active.clear();
    age_frames.clear();
    pos_x.clear();
    pos_y.clear();
    vel_x.clear();
    vel_y.clear();
    scale.clear();
    initial_scale.clear();
    rotation.clear();
    angular_speed.clear();
    color_r.clear();
    color_g.clear();
    color_b.clear();
    color_a.clear();
    initial_color_r.clear();
    initial_color_g.clear();
    initial_color_b.clear();
    initial_color_a.clear();
    while (!free_list.empty())
    {
        free_list.pop();
    }
}

void ParticleSystem::Stop()
{
    emission_allowed = false;
}

void ParticleSystem::Play()
{
    emission_allowed = true;
}

void ParticleSystem::Burst()
{
    EmitBurstNow(burst_quantity);
}

void ParticleSystem::EmitBurstNow(int quantity)
{
    for (int i = 0; i < quantity; ++i)
    {
        const float angle_deg = angle_engine.Sample();
        const float angle_rad = glm::radians(angle_deg);
        const float radius = radius_engine.Sample();
        const float spawn_scale = scale_engine.Sample();
        const float rot = rotation_engine.Sample();
        const float speed = speed_engine.Sample();
        const float rot_speed = rotation_speed_engine.Sample();

        const float spawn_x = x + radius * glm::cos(angle_rad);
        const float spawn_y = y + radius * glm::sin(angle_rad);
        const float spawn_vx = speed * glm::cos(angle_rad);
        const float spawn_vy = speed * glm::sin(angle_rad);

        if (!free_list.empty())
        {
            const size_t idx = free_list.front();
            free_list.pop();
            active[idx] = 1;
            age_frames[idx] = 0;
            pos_x[idx] = spawn_x;
            pos_y[idx] = spawn_y;
            vel_x[idx] = spawn_vx;
            vel_y[idx] = spawn_vy;
            scale[idx] = spawn_scale;
            initial_scale[idx] = spawn_scale;
            rotation[idx] = rot;
            angular_speed[idx] = rot_speed;
            color_r[idx] = start_color_r;
            color_g[idx] = start_color_g;
            color_b[idx] = start_color_b;
            color_a[idx] = start_color_a;
            initial_color_r[idx] = start_color_r;
            initial_color_g[idx] = start_color_g;
            initial_color_b[idx] = start_color_b;
            initial_color_a[idx] = start_color_a;
        }
        else
        {
            active.push_back(1);
            age_frames.push_back(0);
            pos_x.push_back(spawn_x);
            pos_y.push_back(spawn_y);
            vel_x.push_back(spawn_vx);
            vel_y.push_back(spawn_vy);
            scale.push_back(spawn_scale);
            initial_scale.push_back(spawn_scale);
            rotation.push_back(rot);
            angular_speed.push_back(rot_speed);
            color_r.push_back(start_color_r);
            color_g.push_back(start_color_g);
            color_b.push_back(start_color_b);
            color_a.push_back(start_color_a);
            initial_color_r.push_back(start_color_r);
            initial_color_g.push_back(start_color_g);
            initial_color_b.push_back(start_color_b);
            initial_color_a.push_back(start_color_a);
        }
    }
}
