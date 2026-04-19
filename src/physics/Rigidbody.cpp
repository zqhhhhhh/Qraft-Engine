#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include "physics/Rigidbody.h"
#include "physics/Raycast.h"
#include "engine/Actor.h"
#include "engine/ComponentDB.h"

std::unique_ptr<b2World> Rigidbody::physics_world = nullptr;
std::vector<Rigidbody*> Rigidbody::all_instances;

static constexpr float kDegreesToRadians = b2_pi / 180.0f;
static constexpr float kRadiansToDegrees = 180.0f / b2_pi;

namespace
{
    struct PendingCollisionEvent
    {
        Actor* self_actor = nullptr;
        Actor* other_actor = nullptr;
        b2Vec2 point{ -999.0f, -999.0f };
        b2Vec2 relative_velocity{ 0.0f, 0.0f };
        b2Vec2 normal{ -999.0f, -999.0f };
        bool is_enter = true;
        bool is_trigger = false;
    };

    std::vector<PendingCollisionEvent> g_pending_collision_events;
    constexpr uint16 kColliderCategory = 0x0001;
    constexpr uint16 kTriggerCategory = 0x0002;

    Actor* get_actor_from_fixture(b2Fixture* fixture)
    {
        if (fixture == nullptr)
        {
            return nullptr;
        }
        const uintptr_t raw_ptr = fixture->GetUserData().pointer;
        if (raw_ptr == 0)
        {
            return nullptr;
        }
        return reinterpret_cast<Actor*>(raw_ptr);
    }

    void queue_collision_pair(b2Contact* contact, bool is_enter)
    {
        if (contact == nullptr)
        {
            return;
        }
        b2Fixture* fixture_a = contact->GetFixtureA();
        b2Fixture* fixture_b = contact->GetFixtureB();
        Actor* actor_a = get_actor_from_fixture(fixture_a);
        Actor* actor_b = get_actor_from_fixture(fixture_b);
        if (actor_a == nullptr || actor_b == nullptr)
        {
            return;
        }
        const bool is_trigger_pair = fixture_a->IsSensor() && fixture_b->IsSensor();
        const bool is_collision_pair = !fixture_a->IsSensor() && !fixture_b->IsSensor();
        if (!is_trigger_pair && !is_collision_pair)
        {
            return;
        }
        b2Vec2 point(-999.0f, -999.0f);
        b2Vec2 normal(-999.0f, -999.0f);
        const b2Vec2 velocity_a = fixture_a->GetBody()->GetLinearVelocity();
        const b2Vec2 velocity_b = fixture_b->GetBody()->GetLinearVelocity();
        b2Vec2 relative_velocity = velocity_a - velocity_b;
        if (is_enter && is_collision_pair)
        {
            b2WorldManifold manifold;
            contact->GetWorldManifold(&manifold);
            point = manifold.points[0];
            normal = manifold.normal;
        }
        g_pending_collision_events.push_back({ actor_a, actor_b, point, relative_velocity, normal, is_enter, is_trigger_pair });
        g_pending_collision_events.push_back({ actor_b, actor_a, point, relative_velocity, normal, is_enter, is_trigger_pair });
    }

    class RigidbodyContactListener : public b2ContactListener
    {
    public:
        void BeginContact(b2Contact* contact) override
        {
            queue_collision_pair(contact, true);
        }

        void EndContact(b2Contact* contact) override
        {
            queue_collision_pair(contact, false);
        }
    };

    std::unique_ptr<RigidbodyContactListener> g_contact_listener;

    void report_lua_error(const std::string& actor_name, const luabridge::LuaException& e)
    {
        std::cout << "\033[31m" << actor_name << " : " << e.what() << "\033[0m" << std::endl;
    }
}

Rigidbody* Rigidbody::NewInstance()
{
    Rigidbody* rb = new Rigidbody();
    all_instances.push_back(rb);
    return rb;
}

Rigidbody* Rigidbody::CloneInstance(const Rigidbody* parent)
{
    Rigidbody* rb = nullptr;
    if (parent != nullptr)
    {
        rb = new Rigidbody(*parent);
        rb->body = nullptr;
    }
    else
    {
        rb = new Rigidbody();
    }
    all_instances.push_back(rb);
    return rb;
}

void Rigidbody::RegisterWithLua(lua_State* lua_state)
{
    if (lua_state == nullptr)
    {
        return;
    }

    luabridge::getGlobalNamespace(lua_state)
        .beginClass<Rigidbody>("Rigidbody")
        .addConstructor<void (*)(void)>()
        .addProperty("key", &Rigidbody::key)
        .addProperty("type", &Rigidbody::type)
        .addProperty("enabled", &Rigidbody::enabled)
        .addProperty("actor", &Rigidbody::GetActor, &Rigidbody::SetActor)
        .addProperty("x", &Rigidbody::x)
        .addProperty("y", &Rigidbody::y)
        .addProperty("body_type", &Rigidbody::body_type)
        .addProperty("precise", &Rigidbody::precise)
        .addProperty("gravity_scale", &Rigidbody::gravity_scale)
        .addProperty("density", &Rigidbody::density)
        .addProperty("angular_friction", &Rigidbody::angular_friction)
        .addProperty("rotation", &Rigidbody::rotation)
        .addProperty("collider_type", &Rigidbody::collider_type)
        .addProperty("width", &Rigidbody::width)
        .addProperty("height", &Rigidbody::height)
        .addProperty("radius", &Rigidbody::radius)
        .addProperty("trigger_type", &Rigidbody::trigger_type)
        .addProperty("trigger_width", &Rigidbody::trigger_width)
        .addProperty("trigger_height", &Rigidbody::trigger_height)
        .addProperty("trigger_radius", &Rigidbody::trigger_radius)
        .addProperty("friction", &Rigidbody::friction)
        .addProperty("bounciness", &Rigidbody::bounciness)
        .addProperty("has_collider", &Rigidbody::has_collider)
        .addProperty("has_trigger", &Rigidbody::has_trigger)
        .addFunction("OnStart", &Rigidbody::OnStart)
        .addFunction("OnDestroy", &Rigidbody::OnDestroy)
        .addFunction("GetPosition", &Rigidbody::GetPosition)
        .addFunction("GetRotation", &Rigidbody::GetRotation)
        .addFunction("GetVelocity", &Rigidbody::GetVelocity)
        .addFunction("GetAngularVelocity", &Rigidbody::GetAngularVelocity)
        .addFunction("GetGravityScale", &Rigidbody::GetGravityScale)
        .addFunction("GetUpDirection", &Rigidbody::GetUpDirection)
        .addFunction("GetRightDirection", &Rigidbody::GetRightDirection)
        .addFunction("AddForce", &Rigidbody::AddForce)
        .addFunction("SetVelocity", &Rigidbody::SetVelocity)
        .addFunction("SetPosition", &Rigidbody::SetPosition)
        .addFunction("SetRotation", &Rigidbody::SetRotation)
        .addFunction("SetAngularVelocity", &Rigidbody::SetAngularVelocity)
        .addFunction("SetGravityScale", &Rigidbody::SetGravityScale)
        .addFunction("SetUpDirection", &Rigidbody::SetUpDirection)
        .addFunction("SetRightDirection", &Rigidbody::SetRightDirection)
        .endClass();
}

void Rigidbody::Shutdown()
{
    g_pending_collision_events.clear();
    g_contact_listener.reset();
    for (Rigidbody* rb : all_instances)
    {
        delete rb;
    }
    all_instances.clear();
    physics_world.reset();
}

void Rigidbody::StepWorld()
{
    if (!physics_world)
    {
        return;
    }
    physics_world->Step(1.0f / 60.0f, 8, 3);
}

void Rigidbody::DispatchCollisionEvents()
{
    if (g_pending_collision_events.empty())
    {
        return;
    }
    lua_State* L = ComponentDB::GetLuaState();
    if (L == nullptr)
    {
        g_pending_collision_events.clear();
        return;
    }
    for (const PendingCollisionEvent& event : g_pending_collision_events)
    {
        if (event.self_actor == nullptr || event.other_actor == nullptr)
        {
            continue;
        }
        Actor* self_actor = event.self_actor;
        Actor* other_actor = event.other_actor;
        luabridge::LuaRef collision = luabridge::newTable(L);
        collision["other"] = other_actor;
        collision["point"] = event.point;
        collision["relative_velocity"] = event.relative_velocity;
        collision["normal"] = event.normal;
        const auto& callback_map = event.is_trigger
            ? (event.is_enter ? self_actor->components_requiring_ontriggerenter : self_actor->components_requiring_ontriggerexit)
            : (event.is_enter ? self_actor->components_requiring_oncollisionenter : self_actor->components_requiring_oncollisionexit);
        const char* callback_name = event.is_trigger
            ? (event.is_enter ? "OnTriggerEnter" : "OnTriggerExit")
            : (event.is_enter ? "OnCollisionEnter" : "OnCollisionExit");
        for (const auto& [component_key, component_ref_ptr] : callback_map)
        {
            (void)component_key;
            if (!component_ref_ptr)
            {
                continue;
            }
            luabridge::LuaRef enabled_ref = (*component_ref_ptr)["enabled"];
            bool is_enabled = !(enabled_ref.isBool() && !enabled_ref.cast<bool>());
            if (!is_enabled)
            {
                continue;
            }
            luabridge::LuaRef callback = (*component_ref_ptr)[callback_name];
            if (!callback.isFunction())
            {
                continue;
            }
            try
            {
                callback(*component_ref_ptr, collision);
            }
            catch (const luabridge::LuaException& e)
            {
                report_lua_error(self_actor->GetName(), e);
            }
        }
    }
    g_pending_collision_events.clear();
}

void Rigidbody::ClearPendingCollisionEvents()
{
    g_pending_collision_events.clear();
}

void Rigidbody::RemoveForComponent(luabridge::LuaRef component)
{
    if (!physics_world || !component.isUserdata())
    {
        return;
    }
    Rigidbody* rb = component.cast<Rigidbody*>();
    if (rb != nullptr)
    {
        rb->DestroyBody();
    }
}

void Rigidbody::OnDestroy()
{
    DestroyBody();
}

void Rigidbody::DestroyBody()
{
    if (physics_world && body != nullptr)
    {
        physics_world->DestroyBody(body);
        body = nullptr;
    }
}

void Rigidbody::OnStart()
{
    if (!physics_world)
    {
        physics_world = std::make_unique<b2World>(b2Vec2(0.0f, 9.8f));
        if (!g_contact_listener)
        {
            g_contact_listener = std::make_unique<RigidbodyContactListener>();
        }
        physics_world->SetContactListener(g_contact_listener.get());
        Raycast::SetPhysicsWorld(physics_world.get());
    }
    b2BodyDef body_def;
    body_def.position.Set(x, y);
    if (body_type == "static")
    {
        body_def.type = b2_staticBody;
    }
    else if (body_type == "kinematic")
    {
        body_def.type = b2_kinematicBody;
    }
    else
    {
        body_def.type = b2_dynamicBody;
    }
    body_def.bullet = precise;
    body_def.gravityScale = gravity_scale;
    body_def.angularDamping = angular_friction;
    body_def.angle = rotation * kDegreesToRadians;
    body = physics_world->CreateBody(&body_def);
    if (body == nullptr)
    {
        return;
    }
    body->SetLinearVelocity(pending_velocity);
    body->SetAngularVelocity(pending_angular_velocity * kDegreesToRadians);
    const bool no_collider_no_trigger = !has_collider && !has_trigger;

    auto create_fixture = [&](bool sensor, const std::string& shape_type, float shape_width, float shape_height, float shape_radius,
                              uint16 category_bits, uint16 mask_bits)
    {
        b2FixtureDef fixture_def;
        fixture_def.density = density;
        fixture_def.friction = friction;
        fixture_def.restitution = bounciness;
        fixture_def.isSensor = sensor;
        fixture_def.userData.pointer = reinterpret_cast<uintptr_t>(actor);
        fixture_def.filter.categoryBits = category_bits;
        fixture_def.filter.maskBits = mask_bits;

        const bool use_circle_shape = shape_type == "circle" || shape_type == "Circle";
        if (use_circle_shape)
        {
            b2CircleShape circle_shape;
            circle_shape.m_radius = glm::abs(shape_radius);
            fixture_def.shape = &circle_shape;
            body->CreateFixture(&fixture_def);
        }
        else
        {
            b2PolygonShape box_shape;
            box_shape.SetAsBox(shape_width * 0.5f, shape_height * 0.5f);
            fixture_def.shape = &box_shape;
            body->CreateFixture(&fixture_def);
        }
    };
    if (has_collider)
    {
        create_fixture(false, collider_type, width, height, radius, kColliderCategory, kColliderCategory);
    }
    if (has_trigger)
    {
        create_fixture(true, trigger_type, trigger_width, trigger_height, trigger_radius, kTriggerCategory, kTriggerCategory);
    }
    if (no_collider_no_trigger)
    {
        create_fixture(true, "box", width, height, radius, 0x0004, 0x0000);
    }
}

b2Vec2 Rigidbody::GetPosition() const
{
    if (body != nullptr) return body->GetPosition();
    return b2Vec2(x, y);
}

float Rigidbody::GetRotation() const
{
    if (body != nullptr) return body->GetAngle() * kRadiansToDegrees;
    return rotation;
}

b2Vec2 Rigidbody::GetVelocity() const
{
    if (body != nullptr) return body->GetLinearVelocity();
    return pending_velocity;
}

float Rigidbody::GetAngularVelocity() const
{
    if (body != nullptr) return body->GetAngularVelocity() * kRadiansToDegrees;
    return pending_angular_velocity;
}

float Rigidbody::GetGravityScale() const
{
    if (body != nullptr) return body->GetGravityScale();
    return gravity_scale;
}

b2Vec2 Rigidbody::GetUpDirection() const
{
    const float angle = (body != nullptr)
        ? body->GetAngle()
        : (rotation * kDegreesToRadians);
    return b2Vec2(glm::sin(angle), -glm::cos(angle));
}

b2Vec2 Rigidbody::GetRightDirection() const
{
    const float angle = (body != nullptr)
        ? body->GetAngle()
        : (rotation * kDegreesToRadians);
    return b2Vec2(glm::cos(angle), glm::sin(angle));
}

void Rigidbody::AddForce(b2Vec2 force)
{
    if (body != nullptr) body->ApplyForceToCenter(force, true);
}

void Rigidbody::SetVelocity(b2Vec2 vel)
{
    if (body != nullptr)
    {
        body->SetLinearVelocity(vel);
    }
    else
    {
        pending_velocity = vel;
    }
}

void Rigidbody::SetPosition(b2Vec2 pos)
{
    if (body != nullptr)
    {
        body->SetTransform(pos, body->GetAngle());
    }
    else
    {
        x = pos.x;
        y = pos.y;
    }
}

void Rigidbody::SetRotation(float degrees_clockwise)
{
    if (body != nullptr)
    {
        body->SetTransform(body->GetPosition(), degrees_clockwise * kDegreesToRadians);
    }
    else
    {
        rotation = degrees_clockwise;
    }
}

void Rigidbody::SetAngularVelocity(float degrees_clockwise)
{
    if (body != nullptr)
    {
        body->SetAngularVelocity(degrees_clockwise * kDegreesToRadians);
    }
    else
    {
        pending_angular_velocity = degrees_clockwise;
    }
}

void Rigidbody::SetGravityScale(float scale)
{
    if (body != nullptr)
    {
        body->SetGravityScale(scale);
    }
    else
    {
        gravity_scale = scale;
    }
}

void Rigidbody::SetUpDirection(b2Vec2 direction)
{
    direction.Normalize();
    const float angle = glm::atan(direction.x, -direction.y);
    if (body != nullptr)
    {
        body->SetTransform(body->GetPosition(), angle);
    }
    else
    {
        rotation = angle * kRadiansToDegrees;
    }
}

void Rigidbody::SetRightDirection(b2Vec2 direction)
{
    direction.Normalize();
    const float angle = glm::atan(direction.x, -direction.y) - static_cast<float>(b2_pi) / 2.0f;
    if (body != nullptr)
    {
        body->SetTransform(body->GetPosition(), angle);
    }
    else
    {
        rotation = angle * kRadiansToDegrees;
    }
}
