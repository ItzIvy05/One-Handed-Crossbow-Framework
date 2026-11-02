#include "Player.h"
#include "Fire.h"
#include "Forms.h"

namespace {
    // OnControlDown and OnControlUp for "Left Attack/Block" and "Right Attack/Block"
    bool Handle(RE::ButtonEvent* a_event, bool a_left)
    {
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!a_event || !Forms::GetHandWeapon(player, a_left) || !player->AsActorState()->IsWeaponDrawn()) {
            return false;
        }
        // right attack while blocking is the bash
        if (!a_left && player->AsActorState()->actorState2.wantBlocking) {
            return false;
        }
        if (a_event->IsDown()) {
            Fire::StartLaunch(player, a_left);
        } else if (a_event->IsUp()) {
            Fire::EndLaunch(player, a_left);
        }
        return true;
    }

    template <bool Left>
    struct Attack {
        static void thunk(RE::AttackBlockHandler* a_handler, RE::ButtonEvent* a_event)
        {
            if (!Handle(a_event, Left)) {
                func(a_handler, a_event);
            }
        }
        static inline REL::Relocation<decltype(thunk)> func;
    };

    struct DualAttack {
        static void thunk(RE::AttackBlockHandler* a_handler, RE::ButtonEvent* a_event)
        {
            const auto player = RE::PlayerCharacter::GetSingleton();
            const bool drawn = player->AsActorState()->IsWeaponDrawn();
            if (!drawn || (!Forms::GetHandWeapon(player, true) && !Forms::GetHandWeapon(player, false))) {
                func(a_handler, a_event);
                return;
            }
            Attack<true>::thunk(a_handler, a_event);
            Attack<false>::thunk(a_handler, a_event);
        }
        static inline REL::Relocation<decltype(thunk)> func;
    };
}

namespace Player {
    void Install()
    {
        const bool latest = REL::Module::get().version() >= SKSE::RUNTIME_SSE_1_7_104;
        auto& trampoline = SKSE::GetTrampoline();
        const auto process = RELOCATION_ID(41347, 42421).address();  // AttackBlockHandler::ProcessButton
        DualAttack::func = trampoline.write_call<5>(process + (latest ? 0xC7 : 0xC1), DualAttack::thunk);
        Attack<true>::func = trampoline.write_call<5>(process + (latest ? 0xD2 : 0xCC), Attack<true>::thunk);
        Attack<false>::func = trampoline.write_call<5>(process + (latest ? 0xDD : 0xD7), Attack<false>::thunk);
        trampoline.write_call<5>(process + REL::Relocate(0x156, latest ? 0x1BE : 0x1A3), DualAttack::thunk);
        trampoline.write_call<5>(process + REL::Relocate(0x1E1, latest ? 0x249 : 0x228), Attack<true>::thunk);
        trampoline.write_call<5>(process + REL::Relocate(0x21E, latest ? 0x286 : 0x262), Attack<false>::thunk);
    }
}
