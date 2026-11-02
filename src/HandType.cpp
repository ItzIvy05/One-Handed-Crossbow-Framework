#include "HandType.h"
#include "Forms.h"

#undef GetObject

namespace {
    std::array<RE::BGSAction*, 3> rightActions{};
    std::array<RE::BGSAction*, 3> leftActions{};
    std::array<RE::BGSAction*, 3> dualActions{};

    bool Contains(const std::array<RE::BGSAction*, 3>& a_actions, const RE::BGSAction* a_action)
    {
        for (const auto action : a_actions) {
            if (action && action == a_action) {
                return true;
            }
        }
        return false;
    }

    // same item type the engine reports, staff for hand crossbows and pistols
    struct EquippedItemType {
        static std::uint16_t thunk(RE::TESForm* a_form)
        {
            using func_t = void*(RE::TESForm*);
            static REL::Relocation<func_t> getEquipType{ REL::ID(14218) };  // TESBoundObject::GetEquipType
            const auto equipType = getEquipType(a_form);
            if (!a_form || !equipType) {
                return 0;
            }

            switch (a_form->GetFormType()) {
            case RE::FormType::Armor:
                return 10;
            case RE::FormType::Light:
                return 11;
            case RE::FormType::Weapon:
                {
                    if (Forms::GetVariant(a_form)) {
                        return 8;
                    }
                    const auto type = static_cast<std::int8_t>(a_form->As<RE::TESObjectWEAP>()->weaponData.animationType.underlying());
                    return static_cast<std::uint16_t>(type < 9 ? type : type + 3);
                }
            case RE::FormType::Spell:
            case RE::FormType::Scroll:
                return 9;
            default:
                return 0;
            }
        }
    };

    // the idle lookup for an action, where the vanilla idle conditions are evaluated
    struct FindIdle {
        static bool thunk(void* a_manager, RE::BGSActionData* a_data)
        {
            const auto source = a_data ? a_data->source.get() : nullptr;
            const auto actor = source ? source->As<RE::Actor>() : nullptr;
            if (actor) {
                const auto action = a_data->action;
                const bool right = Forms::GetHandWeapon(actor, false) != nullptr;
                const bool left = Forms::GetHandWeapon(actor, true) != nullptr;
                // right attack while blocking is the bash
                if (right && Contains(rightActions, action) && !actor->AsActorState()->actorState2.wantBlocking) {
                    return false;
                }
                if (left && Contains(leftActions, action)) {
                    return false;
                }
                if ((right || left) && Contains(dualActions, action)) {
                    return false;
                }
            }
            return func(a_manager, a_data);
        }
        static inline REL::Relocation<decltype(thunk)> func;
    };
}

namespace HandType {
    void Install()
    {
        auto& trampoline = SKSE::GetTrampoline();
        trampoline.write_branch<6>(REL::ID(14220).address(), EquippedItemType::thunk);  // TESForm::GetEquippedItemTypeId
        FindIdle::func = trampoline.write_call<5>(REL::ID(38949).address() + 0x73, FindIdle::thunk);  // action executor -> idle lookup
    }

    void LoadActions()
    {
        const auto objects = RE::BGSDefaultObjectManager::GetSingleton();
        rightActions = { objects->GetObject<RE::BGSAction>(RE::DEFAULT_OBJECT::kActionRightAttack), objects->GetObject<RE::BGSAction>(RE::DEFAULT_OBJECT::kActionRightRelease), objects->GetObject<RE::BGSAction>(RE::DEFAULT_OBJECT::kActionRightPowerAttack) };
        leftActions = { objects->GetObject<RE::BGSAction>(RE::DEFAULT_OBJECT::kActionLeftAttack), objects->GetObject<RE::BGSAction>(RE::DEFAULT_OBJECT::kActionLeftRelease), objects->GetObject<RE::BGSAction>(RE::DEFAULT_OBJECT::kActionLeftPowerAttack) };
        dualActions = { objects->GetObject<RE::BGSAction>(RE::DEFAULT_OBJECT::kActionDualAttack), objects->GetObject<RE::BGSAction>(RE::DEFAULT_OBJECT::kActionDualRelease), objects->GetObject<RE::BGSAction>(RE::DEFAULT_OBJECT::kActionDualPowerAttack) };
    }
}
