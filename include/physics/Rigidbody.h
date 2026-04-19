#pragma once

#include <memory>
#include <string>
#include <vector>
#include "glm/glm.hpp"
#include "lua/lua.hpp"
#include "LuaBridge/LuaBridge.h"
#include "box2d/box2d.h"

class Actor;

class Rigidbody
{
public:
    Rigidbody() = default;

    // Component identity (set by the engine)
    std::string key;
    std::string type = "Rigidbody";
    bool enabled = true;
    Actor* actor = nullptr;

    // Physics properties (readable/writable from Lua)
    float x = 0.0f;
    float y = 0.0f;
    std::string body_type = "dynamic";
    bool precise = true;
    float gravity_scale = 1.0f;
    float density = 1.0f;
    float angular_friction = 0.3f;
    float rotation = 0.0f;   // clockwise degrees
    std::string collider_type = "box";
    float width = 1.0f;
    float height = 1.0f;
    float radius = 0.5f;
    std::string trigger_type = "box";
    float trigger_width = 1.0f;
    float trigger_height = 1.0f;
    float trigger_radius = 0.5f;
    float friction = 0.3f;
    float bounciness = 0.3f;
    bool has_collider = true;
    bool has_trigger = true;

    // Lifecycle / Lua-callable methods
    void OnStart();
    void OnDestroy();

    // Getters
    b2Vec2 GetPosition() const;
    float  GetRotation() const;          // clockwise degrees
    b2Vec2 GetVelocity() const;
    float  GetAngularVelocity() const;   // clockwise degrees/sec
    float  GetGravityScale() const;
    b2Vec2 GetUpDirection() const;
    b2Vec2 GetRightDirection() const;

    // Setters / manipulators
    void AddForce(b2Vec2 force);
    void SetVelocity(b2Vec2 vel);
    void SetPosition(b2Vec2 pos);
    void SetRotation(float degrees_clockwise);
    void SetAngularVelocity(float degrees_clockwise);
    void SetGravityScale(float scale);
    void SetUpDirection(b2Vec2 direction);
    void SetRightDirection(b2Vec2 direction);

    // Accessor helpers for LuaBridge (Actor* needs explicit getter/setter)
    Actor* GetActor() const { return actor; }
    void   SetActor(Actor* a) { actor = a; }

    // ---- Static engine interface ----
    static Rigidbody* NewInstance();
    static Rigidbody* CloneInstance(const Rigidbody* parent);
    static void RegisterWithLua(lua_State* lua_state);
    static void Shutdown();
    static void StepWorld();
    static void DispatchCollisionEvents();
    static void ClearPendingCollisionEvents();
    static void RemoveForComponent(luabridge::LuaRef component);

private:
    void DestroyBody();

    b2Body* body = nullptr;
    b2Vec2 pending_velocity{ 0.0f, 0.0f };
    float pending_angular_velocity = 0.0f;

    static std::unique_ptr<b2World> physics_world;
    static std::vector<Rigidbody*> all_instances;
};
