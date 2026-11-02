#pragma once

#include "Forms.h"

namespace Ammo {
    void Register();
    void OnGameLoaded();

    void Assign(RE::Actor* a_actor, const Forms::Variant& a_variant);
    void Look(RE::Actor* a_actor, const Forms::Variant& a_variant);
    bool CheckForFailure(RE::Actor* a_actor, const Forms::Variant& a_variant, bool a_left);

    void ShowCounter(RE::Actor* a_actor, RE::TESAmmo* a_ammo);
    void HideCounter(RE::Actor* a_actor);

    void CheckInvalidAmmo2H(RE::Actor* a_actor);

    void InventoryEntry(RE::GFxMovieView* a_view, RE::GFxValue* a_object, RE::InventoryEntryData* a_item);
}
