#include <algorithm>
#include "physics/Raycast.h"
#include "engine/Actor.h"
#include "engine/ComponentDB.h"

b2World* Raycast::physics_world = nullptr;

void Raycast::SetPhysicsWorld(b2World* world)
{
    physics_world = world;
}

namespace
{
    struct RaycastHit
    {
        HitResult result;
        float distance = 0.0f;
    };

    std::vector<RaycastHit> g_raycast_hits;

    class CustomRaycastCallback : public b2RayCastCallback
    {
    public:
        float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float fraction) override
        {
            if (fixture == nullptr)
            {
                return -1.0f;
            }
            // Get actor from fixture userData
            const uintptr_t raw_ptr = fixture->GetUserData().pointer;
            if (raw_ptr == 0)
            {
                return -1.0f;
            }
            Actor* actor = reinterpret_cast<Actor*>(raw_ptr);
            if (actor == nullptr)
            {
                return -1.0f;
            }
            // Skip fixtures that opted out of all interactions (phantom/mask=0)
            if (fixture->GetFilterData().maskBits == 0x0000)
            {
                return -1.0f;
            }
            RaycastHit hit;
            hit.result.actor = actor;
            hit.result.point = point;
            hit.result.normal = normal;
            hit.result.is_trigger = fixture->IsSensor();
            hit.distance = fraction;
            g_raycast_hits.push_back(hit);
            return -1.0f;  // Continue reporting all fixtures
        }
    };
}

luabridge::LuaRef Raycast::Raycast_Single(b2Vec2 pos, b2Vec2 dir, float dist)
{
    lua_State* L = ComponentDB::GetLuaState();
    luabridge::LuaRef nil_ref(L);  // nil

    if (!physics_world || dist <= 0.0f)
    {
        return nil_ref;
    }

    // Normalize direction and compute end point
    dir.Normalize();
    b2Vec2 end_pos = pos + dist * dir;

    // Perform raycast collecting all hits
    g_raycast_hits.clear();
    CustomRaycastCallback callback;
    physics_world->RayCast(&callback, pos, end_pos);

    if (g_raycast_hits.empty())
    {
        return nil_ref;
    }

    // Return the closest hit
    std::sort(g_raycast_hits.begin(), g_raycast_hits.end(),
        [](const RaycastHit& a, const RaycastHit& b) { return a.distance < b.distance; });

    const HitResult& hit = g_raycast_hits[0].result;
    luabridge::LuaRef result = luabridge::newTable(L);
    result["actor"] = hit.actor;
    result["point"] = hit.point;
    result["normal"] = hit.normal;
    result["is_trigger"] = hit.is_trigger;
    return result;
}

luabridge::LuaRef Raycast::Raycast_All(b2Vec2 pos, b2Vec2 dir, float dist)
{
    lua_State* L = ComponentDB::GetLuaState();
    luabridge::LuaRef results_table = luabridge::newTable(L);  // empty table

    if (!physics_world || dist <= 0.0f)
    {
        return results_table;
    }

    // Normalize direction and compute end point
    dir.Normalize();
    b2Vec2 end_pos = pos + dist * dir;

    // Perform raycast collecting all hits
    g_raycast_hits.clear();
    CustomRaycastCallback callback;
    physics_world->RayCast(&callback, pos, end_pos);

    if (g_raycast_hits.empty())
    {
        return results_table;
    }

    // Sort nearest to furthest
    std::sort(g_raycast_hits.begin(), g_raycast_hits.end(),
        [](const RaycastHit& a, const RaycastHit& b) { return a.distance < b.distance; });

    // Fill 1-indexed Lua table
    int index = 1;
    for (const RaycastHit& hit : g_raycast_hits)
    {
        luabridge::LuaRef hit_table = luabridge::newTable(L);
        hit_table["actor"] = hit.result.actor;
        hit_table["point"] = hit.result.point;
        hit_table["normal"] = hit.result.normal;
        hit_table["is_trigger"] = hit.result.is_trigger;
        results_table[index++] = hit_table;
    }

    return results_table;
}
