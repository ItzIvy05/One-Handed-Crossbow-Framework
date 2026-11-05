#include "Skill.h"
#include "Forms.h"

namespace {
    bool HoldsStaff(RE::Actor* a_actor)
    {
        for (const bool left : { false, true }) {
            const auto object = a_actor->GetEquippedObject(left);
            const auto weapon = object ? object->As<RE::TESObjectWEAP>() : nullptr;
            if (weapon && weapon->IsStaff()) {
                return true;
            }
        }
        return false;
    }

    // staff xp from other mods lands here because the crossbow reports itself as a staff
    struct UseSkill {
        static void thunk(RE::PlayerCharacter* a_this, RE::ActorValue a_skill, float a_points, RE::TESForm* a_object, std::uint32_t a_action)
        {
            if (a_skill == RE::ActorValue::kEnchanting && !RE::UI::GetSingleton()->GameIsPaused() && (Forms::GetHandWeapon(a_this, false) || Forms::GetHandWeapon(a_this, true)) && !HoldsStaff(a_this)) {
                a_skill = RE::ActorValue::kArchery;
            }
            func(a_this, a_skill, a_points, a_object, a_action);
        }
        static inline REL::Relocation<decltype(thunk)> func;
    };
}

namespace Skill {
    void Install()
    {
        REL::Relocation<std::uintptr_t> player{ RE::VTABLE_PlayerCharacter[0] };
        UseSkill::func = player.write_vfunc(0xF7, UseSkill::thunk);  // Actor::UseSkill
    }
}
