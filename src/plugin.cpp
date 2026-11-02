#include "Ammo.h"
#include "Fire.h"
#include "Forms.h"
#include "HandType.h"
#include "NPC.h"
#include "Player.h"

namespace {
    void OnMessage(SKSE::MessagingInterface::Message* a_message)
    {
        switch (a_message->type) {
        case SKSE::MessagingInterface::kDataLoaded:
            Forms::Load();
            HandType::LoadActions();
            HandType::Install();
            Player::Install();
            Fire::Install();
            NPC::Install();
            Ammo::Register();
            break;
        case SKSE::MessagingInterface::kPreLoadGame:
            State::Clear();
            break;
        case SKSE::MessagingInterface::kNewGame:
            State::Clear();
            Ammo::OnGameLoaded();
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
            Ammo::OnGameLoaded();
            break;
        default:
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse, { .trampoline = true, .trampolineSize = 14 * 5 + 8 });
    Forms::ReadConfig();
    SKSE::GetScaleformInterface()->Register(Ammo::InventoryEntry);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}
