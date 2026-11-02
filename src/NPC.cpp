#include "NPC.h"
#include "Fire.h"
#include "Forms.h"

namespace {
    constexpr float aimTime = 0.25f;

    float conditionInterval = 1.0f;

    // slowed while aiming, like OHCF_Spell_NPCSlow in the control script
    struct AimTask {
        RE::ActorHandle handle;
        bool left;
        bool start;

        void operator()() const
        {
            const auto actor = handle.get();
            if (!actor) {
                return;
            }
            if (start) {
                actor->AddSpell(Forms::npcSlow);
                Fire::StartLaunch(actor.get(), left);
            } else {
                Fire::EndLaunch(actor.get(), left);
                actor->RemoveSpell(Forms::npcSlow);
            }
        }
    };

    bool Holds(RE::Actor* a_actor, const Forms::Variant& a_variant, bool a_left)
    {
        return Forms::GetVariant(Forms::GetHandWeapon(a_actor, a_left)) == &a_variant;
    }

    // OHCF_Spell_NPCControl conditions
    bool ConditionsHold(RE::Actor* a_actor, RE::Actor* a_target)
    {
        const float distance = a_actor->GetPosition().GetDistance(a_target->GetPosition());
        if (distance <= Forms::npcDistanceMin->value || distance >= Forms::npcDistanceMax->value) {
            return false;
        }
        return a_actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina) > Forms::staminaCostAmount->value;
    }

    // OHCF_MGEF_NPCControlScript, right hand then left hand, 0.25 seconds each, then the cooldown
    void Tick(RE::Character* a_actor, float a_delta)
    {
        if (!Forms::GetHandWeapon(a_actor, false) && !Forms::GetHandWeapon(a_actor, true)) {
            return;
        }

        const auto tasks = SKSE::GetTaskInterface();
        std::scoped_lock guard(State::lock);
        auto& data = State::Get(a_actor);
        data.cooldown = std::max(0.0f, data.cooldown - a_delta);

        bool busy = false;
        for (const auto variant : Forms::variants) {
            auto& cycle = data.cycle[variant->index];
            if (cycle.hand == 0) {
                continue;
            }
            busy = true;
            cycle.release -= a_delta;
            if (cycle.release > 0.0f) {
                continue;
            }
            tasks->AddTask(AimTask{ a_actor->GetHandle(), cycle.hand == 2, false });
            if (cycle.hand == 1 && Holds(a_actor, *variant, true)) {
                tasks->AddTask(AimTask{ a_actor->GetHandle(), true, true });
                cycle.hand = 2;
                cycle.release = aimTime;
            } else {
                cycle.hand = 0;
                data.cooldown = Forms::npcCooldown;
            }
        }
        if (busy) {
            return;
        }

        data.poll -= a_delta;
        if (data.poll > 0.0f) {
            return;
        }
        data.poll = conditionInterval;

        if (data.cooldown > 0.0f || a_actor->IsDead() || !a_actor->IsInCombat()) {
            return;
        }
        const auto target = a_actor->GetActorRuntimeData().currentCombatTarget.get();
        if (!target || !ConditionsHold(a_actor, target.get())) {
            return;
        }

        bool unused = false;
        const bool lineOfSight = a_actor->HasLineOfSight(target.get(), unused);
        for (const auto variant : Forms::variants) {
            const bool right = Holds(a_actor, *variant, false);
            const bool left = Holds(a_actor, *variant, true);
            if (!right && !left) {
                continue;
            }
            if (!lineOfSight) {
                data.cooldown = Forms::npcCooldown;
                continue;
            }
            auto& cycle = data.cycle[variant->index];
            cycle.hand = right ? 1 : 2;
            cycle.release = aimTime;
            tasks->AddTask(AimTask{ a_actor->GetHandle(), !right, true });
        }
    }

    struct Update {
        static void thunk(RE::Character* a_this, float a_delta)
        {
            func(a_this, a_delta);
            Tick(a_this, RE::GetSecondsSinceLastFrame());  // npcs are updated with a zero delta
        }
        static inline REL::Relocation<decltype(thunk)> func;
    };
}

namespace NPC {
    void Install()
    {
        const auto setting = RE::GameSettingCollection::GetSingleton()->GetSetting("fActiveEffectConditionUpdateInterval");
        if (setting) {
            conditionInterval = setting->GetFloat();
        }

        REL::Relocation<std::uintptr_t> character{ RE::VTABLE_Character[0] };
        Update::func = character.write_vfunc(0xAD, Update::thunk);  // Actor::Update
    }
}
