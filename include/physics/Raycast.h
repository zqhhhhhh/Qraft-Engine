#pragma once

#include <vector>
#include "box2d/box2d.h"
#include "lua/lua.hpp"
#include "LuaBridge/LuaBridge.h"

class Actor;

struct HitResult
{
    Actor* actor = nullptr;
    b2Vec2 point{0.0f, 0.0f};
    b2Vec2 normal{0.0f, 0.0f};
    bool is_trigger = false;
};

class Raycast
{
public:
    static void SetPhysicsWorld(b2World* world);
    static luabridge::LuaRef Raycast_Single(b2Vec2 pos, b2Vec2 dir, float dist);
    static luabridge::LuaRef Raycast_All(b2Vec2 pos, b2Vec2 dir, float dist);

private:
    static b2World* physics_world;
};
