#include "Ammo.h"
#include "Fire.h"

namespace {
    bool HasKeyword(RE::TESForm* a_form, RE::BGSKeyword* a_keyword)
    {
        const auto keywordForm = a_form ? a_form->As<RE::BGSKeywordForm>() : nullptr;
        return keywordForm && keywordForm->HasKeyword(a_keyword);
    }

    // GetNthFormWithKeyword(actor, keyword, 0) from Inventory Functions, same visit order
    RE::TESAmmo* FindAmmo(RE::Actor* a_actor, RE::BGSKeyword* a_keyword)
    {
        std::vector<RE::InventoryEntryData*> changes;
        std::unordered_map<RE::TESBoundObject*, std::size_t> lastChange;
        const auto inventory = a_actor->GetInventoryChanges(true);
        if (inventory && inventory->entryList) {
            for (const auto entry : *inventory->entryList) {
                if (entry) {
                    changes.push_back(entry);
                    lastChange[entry->object] = changes.size() - 1;
                }
            }
        }

        const auto container = a_actor->GetContainer();
        if (container) {
            for (std::uint32_t i = 0; i < container->numContainerObjects; ++i) {
                const auto object = container->containerObjects[i];
                if (!object || !object->obj || object->obj->Is(RE::FormType::LeveledItem)) {
                    continue;
                }
                std::int32_t count = object->count;
                const auto change = lastChange.find(object->obj);
                if (change != lastChange.end()) {
                    if (changes[change->second]) {
                        count += changes[change->second]->countDelta;
                    }
                    changes[change->second] = nullptr;
                }
                if (count > 0 && HasKeyword(object->obj, a_keyword)) {
                    return object->obj->As<RE::TESAmmo>();
                }
            }
        }

        for (const auto entry : changes) {
            if (entry && entry->countDelta > 0 && HasKeyword(entry->object, a_keyword)) {
                return entry->object->As<RE::TESAmmo>();
            }
        }
        return nullptr;
    }

    std::int32_t Count(RE::Actor* a_actor, RE::TESBoundObject* a_object)
    {
        const auto counts = a_actor->GetInventoryCounts();
        const auto count = counts.find(a_object);
        return count != counts.end() ? count->second : 0;
    }

    void GiveOrDrop(RE::Actor* a_actor, RE::TESBoundObject* a_object)
    {
        const auto count = Count(a_actor, a_object);
        if (Forms::IsFollower(a_actor)) {
            a_actor->RemoveItem(a_object, count, RE::ITEM_REMOVE_REASON::kStoreInContainer, nullptr, RE::PlayerCharacter::GetSingleton());
            RE::SendHUDMessage::ShowInventoryChangeMessage(a_object, count, true, true);
            return;
        }
        const auto dropped = a_actor->DropObject(a_object, nullptr, count).get();
        if (dropped) {
            dropped->formFlags |= RE::TESObjectREFR::RecordFlags::kNoAIAcquire;
        }
    }

    void RemoveWeapons(RE::Actor* a_actor, const Forms::Variant& a_variant)
    {
        if (a_actor->IsPlayerRef() || Forms::npcsDropInvalidWeapons->value != 1.0f) {
            return;
        }
        for (const bool left : { true, false }) {
            const auto weapon = Forms::GetHandWeapon(a_actor, left);
            if (weapon && Forms::GetVariant(weapon) == &a_variant) {
                GiveOrDrop(a_actor, weapon);
            }
        }
    }

    void FailNoAmmo(RE::Actor* a_actor, const Forms::Variant& a_variant)
    {
        a_actor->NotifyAnimationGraph("CastStop");
        if (a_actor->IsPlayerRef()) {
            Forms::ShowMessage(a_variant.noAmmoMessage);
        } else {
            RemoveWeapons(a_actor, a_variant);
        }
    }

    void FailNoStamina(RE::Actor* a_actor)
    {
        if (a_actor->IsPlayerRef()) {
            Forms::ShowMessage(Forms::noStamina);
        }
        a_actor->NotifyAnimationGraph("CastStop");
    }

    void DisableStaminaDrain(RE::Actor* a_actor, bool a_left)
    {
        if (Forms::staminaCostEnabled->value == 1.0f) {
            Fire::SetHoldDown(a_actor, a_left, 0.0f);
        }
    }

    bool Wears(RE::Actor* a_actor, const Forms::Variant& a_variant)
    {
        return Forms::GetVariant(Forms::GetHandWeapon(a_actor, false)) == &a_variant || Forms::GetVariant(Forms::GetHandWeapon(a_actor, true)) == &a_variant;
    }

    void ToggleCounter(RE::Actor* a_actor, const Forms::Variant& a_variant, RE::TESAmmo* a_ammo)
    {
        if (!a_actor->IsPlayerRef()) {
            return;
        }
        if (FindAmmo(a_actor, a_variant.ammoKeyword)) {
            Ammo::ShowCounter(a_actor, a_ammo);
        } else {
            Ammo::HideCounter(a_actor);
        }
    }

    void OnEquip(RE::Actor* a_actor, RE::TESForm* a_object, bool a_equipped)
    {
        const bool holds = Forms::GetHandWeapon(a_actor, false) || Forms::GetHandWeapon(a_actor, true);
        if (!holds && !Forms::GetVariant(a_object)) {
            return;
        }

        std::scoped_lock guard(State::lock);
        if (a_object->Is(RE::FormType::Weapon)) {
            State::Get(a_actor).aiming = { false, false };
        }
        const auto ammo = a_object->As<RE::TESAmmo>();
        const bool player = a_actor->IsPlayerRef();
        for (const auto variant : Forms::variants) {
            if (a_equipped) {
                if (!Wears(a_actor, *variant)) {
                    continue;
                }
                Ammo::Assign(a_actor, *variant);
                if (ammo && HasKeyword(ammo, variant->ammoKeyword)) {
                    ToggleCounter(a_actor, *variant, ammo);
                }
            } else if (Wears(a_actor, *variant)) {
                if (!player) {
                    continue;
                }
                if (ammo && a_actor->GetCurrentAmmo()) {
                    Ammo::HideCounter(a_actor);
                }
                for (const bool left : { true, false }) {
                    const auto object = a_actor->GetEquippedObject(left);
                    const auto weapon = object ? object->As<RE::TESObjectWEAP>() : nullptr;
                    if (weapon && !weapon->HasKeyword(variant->weaponKeyword)) {
                        Fire::SetHoldDown(a_actor, left, 0.0f);
                    }
                }
            } else if (player && HasKeyword(a_object, variant->weaponKeyword)) {
                Ammo::HideCounter(a_actor);
                Fire::SetHoldDown(a_actor, true, 0.0f);
                Fire::SetHoldDown(a_actor, false, 0.0f);
            }
        }
    }

    struct EquipSink : RE::BSTEventSink<RE::TESEquipEvent> {
        RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override
        {
            const auto reference = a_event ? a_event->actor.get() : nullptr;
            const auto actor = reference ? reference->As<RE::Actor>() : nullptr;
            const auto object = actor ? RE::TESForm::LookupByID(a_event->baseObject) : nullptr;
            if (object) {
                OnEquip(actor, object, a_event->equipped);
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    struct ContainerSink : RE::BSTEventSink<RE::TESContainerChangedEvent> {
        RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* a_event, RE::BSTEventSource<RE::TESContainerChangedEvent>*) override
        {
            const auto player = RE::PlayerCharacter::GetSingleton();
            if (!a_event || a_event->newContainer != player->GetFormID()) {
                return RE::BSEventNotifyControl::kContinue;
            }
            const auto object = RE::TESForm::LookupByID(a_event->baseObj);
            const auto ammo = object ? object->As<RE::TESAmmo>() : nullptr;
            if (!ammo) {
                return RE::BSEventNotifyControl::kContinue;
            }
            if (player->GetCurrentAmmo() != ammo) {
                return RE::BSEventNotifyControl::kContinue;
            }
            std::scoped_lock guard(State::lock);
            for (const auto variant : Forms::variants) {
                if (Wears(player, *variant) && HasKeyword(ammo, variant->ammoKeyword)) {
                    Ammo::ShowCounter(player, ammo);
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    EquipSink equipSink;
    ContainerSink containerSink;

    struct CounterTask {
        std::int32_t count;
        bool hide;
        std::string name;

        void operator()() const
        {
            const auto hud = RE::UI::GetSingleton()->GetMovieView(RE::HUDMenu::MENU_NAME);
            if (!hud) {
                return;
            }
            std::array<RE::GFxValue, 3> args{ RE::GFxValue(static_cast<double>(count)), RE::GFxValue(hide), RE::GFxValue(name.c_str()) };
            hud->Invoke("_root.HUDMovieBaseInstance.ShowArrowCount", nullptr, args.data(), static_cast<std::uint32_t>(args.size()));
        }
    };

    void StopAndDrop(RE::Actor* a_actor, RE::BGSMessage* a_message, RE::TESAmmo* a_ammo)
    {
        a_actor->NotifyAnimationGraph("attackStop");
        if (a_actor->IsPlayerRef()) {
            Forms::ShowMessage(a_message);
        } else if (Forms::npcsDropInvalidWeapons->value == 1.0f) {
            GiveOrDrop(a_actor, a_ammo);
        }
    }
}

namespace Ammo {
    void Register()
    {
        const auto events = RE::ScriptEventSourceHolder::GetSingleton();
        events->AddEventSink<RE::TESEquipEvent>(&equipSink);
        events->AddEventSink<RE::TESContainerChangedEvent>(&containerSink);
    }

    // the stamina drain effects, spid gave these to every actor, and the hold down globals are saved with the game
    void OnGameLoaded()
    {
        const auto player = RE::PlayerCharacter::GetSingleton();
        for (const auto spell : { Forms::monitorCrossbow, Forms::monitorPistol }) {
            if (!player->HasSpell(spell)) {
                player->AddSpell(spell);
            }
        }
        Fire::SetHoldDown(player, true, 0.0f);
        Fire::SetHoldDown(player, false, 0.0f);
    }

    void Assign(RE::Actor* a_actor, const Forms::Variant& a_variant)
    {
        if (!a_actor->IsPlayerRef()) {
            Look(a_actor, a_variant);
            return;
        }
        if (!a_actor->GetCurrentAmmo()) {
            Look(a_actor, a_variant);
        }
        const auto current = a_actor->GetCurrentAmmo();
        if (current) {
            State::Get(a_actor).ammo[a_variant.index] = current;
        }
    }

    void Look(RE::Actor* a_actor, const Forms::Variant& a_variant)
    {
        auto& ammo = State::Get(a_actor).ammo[a_variant.index];
        if (ammo && Count(a_actor, ammo) > 0) {
            return;
        }
        ammo = FindAmmo(a_actor, a_variant.ammoKeyword);
        if (!ammo) {
            RemoveWeapons(a_actor, a_variant);
            return;
        }
        if (a_actor->IsPlayerRef()) {
            RE::ActorEquipManager::GetSingleton()->EquipObject(a_actor, ammo, nullptr, 1, nullptr, true, false, false);
        }
    }

    bool CheckForFailure(RE::Actor* a_actor, const Forms::Variant& a_variant, bool a_left)
    {
        const auto ammo = State::Get(a_actor).ammo[a_variant.index];
        if (a_actor->IsPlayerRef()) {
            if (!ammo || !ammo->HasKeyword(a_variant.ammoKeyword)) {
                FailNoAmmo(a_actor, a_variant);
                DisableStaminaDrain(a_actor, a_left);
                return true;
            }
            if (Forms::staminaCostEnabled->value == 1.0f && a_actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina) < Forms::staminaCostAmount->value) {
                FailNoStamina(a_actor);
                DisableStaminaDrain(a_actor, a_left);
                return true;
            }
            return false;
        }
        if (FindAmmo(a_actor, a_variant.ammoKeyword)) {
            Look(a_actor, a_variant);
            return false;
        }
        FailNoAmmo(a_actor, a_variant);
        return true;
    }

    void ShowCounter(RE::Actor* a_actor, RE::TESAmmo* a_ammo)
    {
        if (a_actor->IsPlayerRef() && a_ammo) {
            SKSE::GetTaskInterface()->AddUITask(CounterTask{ Count(a_actor, a_ammo), false, a_ammo->GetName() });
        }
    }

    void HideCounter(RE::Actor* a_actor)
    {
        if (a_actor->IsPlayerRef()) {
            SKSE::GetTaskInterface()->AddUITask(CounterTask{ 0, true, "No Ammo" });
        }
    }

    // skyui picks the crossbow subtype and icon from weaponType 9
    void InventoryEntry(RE::GFxMovieView*, RE::GFxValue* a_object, RE::InventoryEntryData* a_item)
    {
        if (a_object && a_item && Forms::GetVariant(a_item->object)) {
            a_object->SetMember("weaponType", RE::GFxValue(9.0));
        }
    }

    void CheckInvalidAmmo2H(RE::Actor* a_actor)
    {
        const auto object = a_actor->GetEquippedObject(false);
        const auto weapon = object ? object->As<RE::TESObjectWEAP>() : nullptr;
        const auto ammo = a_actor->GetCurrentAmmo();
        if (!weapon || !ammo || weapon->GetWeaponType() != RE::WEAPON_TYPE::kCrossbow) {
            return;
        }

        if (weapon->HasKeyword(Forms::gun2H)) {
            if (!ammo->HasKeyword(Forms::ammoBullet2H)) {
                StopAndDrop(a_actor, Forms::noAmmo2HGun, ammo);
            } else if (weapon->HasKeyword(Forms::gun2HHandgun) && !ammo->HasKeyword(Forms::ammoBullet2HHandgun)) {
                StopAndDrop(a_actor, Forms::noAmmo2HGun, ammo);
            } else if (weapon->HasKeyword(Forms::gun2HRifle) && !ammo->HasKeyword(Forms::gun2HRifle)) {
                StopAndDrop(a_actor, Forms::noAmmo2HGun, ammo);
            } else if (weapon->HasKeyword(Forms::gun2HGrenade) && !ammo->HasKeyword(Forms::gun2HGrenade)) {
                StopAndDrop(a_actor, Forms::noAmmo2HGun, ammo);
            } else if (weapon->HasKeyword(Forms::gun2HBlunderbuss) && !ammo->HasKeyword(Forms::gun2HBlunderbuss)) {
                StopAndDrop(a_actor, Forms::noAmmo2HGun, ammo);
            }
        } else if (ammo->HasKeyword(Forms::ammoBullet2H) || ammo->HasKeyword(Forms::ammoBullet1H)) {
            StopAndDrop(a_actor, Forms::noAmmo2HCrossbow, ammo);
        }
    }
}
