#include "Fire.h"
#include "Ammo.h"
#include "Forms.h"

namespace {
    thread_local bool firingLeft = false;

    bool GetGraphBool(RE::Actor* a_actor, const char* a_name)
    {
        bool value = false;
        a_actor->GetGraphVariableBool(a_name, value);
        return value;
    }

    bool IsNotEquipping(RE::Actor* a_actor)
    {
        return !GetGraphBool(a_actor, "IsEquipping") && a_actor->AsActorState()->IsWeaponDrawn();
    }

    bool IsReadyToFire(RE::Actor* a_actor)
    {
        const auto state = a_actor->AsActorState();
        return !RE::UI::GetSingleton()->GameIsPaused() && !state->IsSprinting() && !state->IsSwimming() && !GetGraphBool(a_actor, "bInJumpState") && a_actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kParalysis) == 0.0f && !a_actor->IsInKillMove() && RE::ControlMap::GetSingleton()->IsLookingControlsEnabled() && !GetGraphBool(a_actor, "IsBlocking") && !GetGraphBool(a_actor, "IsAttacking");
    }

    bool HoldsCrossbow(RE::Actor* a_actor)
    {
        return Forms::GetHandWeapon(a_actor, false) || Forms::GetHandWeapon(a_actor, true);
    }

    RE::AlchemyItem* GetPoison(RE::InventoryEntryData* a_entry)
    {
        using func_t = RE::AlchemyItem*(RE::InventoryEntryData*);
        static REL::Relocation<func_t> func{ REL::ID(15999) };  // InventoryEntryData::GetPoison
        return func(a_entry);
    }

    void DecrementPoisonCount(RE::InventoryEntryData* a_entry)
    {
        using func_t = void(RE::InventoryEntryData*);
        static REL::Relocation<func_t> func{ REL::ID(16000) };  // InventoryEntryData::DecrementPoisonCount
        func(a_entry);
    }

    // MoveRefToCrosshairLoc from Andrealphus' Papyrus Functions
    RE::NiPoint3 CrosshairPoint(RE::Actor* a_actor, bool a_left)
    {
        const float gameX = RE::rad_to_deg(a_actor->GetAngleX());
        const float gameZ = RE::rad_to_deg(a_actor->GetAngleZ());
        const float angleX = RE::deg_to_rad(90.0f + gameX);
        const float angleZ = gameZ < 90.0f ? RE::deg_to_rad(90.0f - gameZ) : RE::deg_to_rad(450.0f - gameZ);

        const bool firstPerson = RE::PlayerCamera::GetSingleton()->IsInFirstPerson();
        float extraX = a_left ? -15.0f : 15.0f;
        float extraZ = 40.0f;
        if (firstPerson) {
            extraX = a_left ? -10.0f : 10.0f;
            extraZ = 10.0f;
        }

        const float distance = Forms::distance->value;
        auto point = a_actor->GetPosition();
        point.x += distance * std::sin(angleX) * std::cos(angleZ) + extraX;
        point.y += distance * std::sin(angleX) * std::sin(angleZ);
        point.z += distance * std::cos(angleX) + Forms::height->value + extraZ;
        return point;
    }

    void PlayShootSound(RE::Actor* a_actor, RE::TESSound* a_sound)
    {
        RE::BSSoundHandle handle;
        if (a_sound->descriptor && RE::BSAudioManager::GetSingleton()->GetSoundHandle(handle, a_sound->descriptor)) {
            handle.SetObjectToFollow(a_actor->Get3D());
            handle.Play();
        }
    }

    RE::SpellItem* GetSpecialSpell(const Forms::Variant& a_variant, RE::TESAmmo* a_ammo)
    {
        const auto& ammo = a_variant.specialAmmo->forms;
        const auto& spells = a_variant.specialSpells->forms;
        for (std::uint32_t i = 0; i < ammo.size() && i < spells.size(); ++i) {
            if (ammo[i] == a_ammo) {
                return spells[i] ? spells[i]->As<RE::SpellItem>() : nullptr;
            }
        }
        return nullptr;
    }

    // LaunchAmmo and LaunchMagicSpell from Andrealphus' Papyrus Functions
    void FireCrossbow(RE::Actor* a_actor, bool a_left)
    {
        const auto weapon = Forms::GetHandWeapon(a_actor, a_left);
        if (!weapon) {
            return;
        }
        const auto variant = Forms::GetVariant(weapon);

        std::scoped_lock guard(State::lock);
        const auto ammo = State::Get(a_actor).ammo[variant->index];
        const auto entry = a_actor->GetEquippedEntryData(a_left);

        RE::EnchantmentItem* enchantment = nullptr;
        const auto candidate = entry ? entry->GetEnchantment() : nullptr;
        if (candidate) {
            const float cost = candidate->CalculateMagickaCost(a_actor);
            const auto charge = a_left ? RE::ActorValue::kLeftItemCharge : RE::ActorValue::kRightItemCharge;
            const auto values = a_actor->AsActorValueOwner();
            if (values->GetActorValue(charge) >= cost) {
                values->DamageActorValue(charge, cost);
                enchantment = candidate;
            } else if (a_actor->IsPlayerRef()) {
                Forms::ShowMessage(Forms::notEnoughCharge);
            }
        }
        const auto poison = entry ? GetPoison(entry) : nullptr;

        PlayShootSound(a_actor, variant->shootSound);

        RE::NiPoint3 targetPoint;
        bool hasTarget = false;
        if (a_actor->IsPlayerRef()) {
            targetPoint = CrosshairPoint(a_actor, a_left);
            hasTarget = true;
        } else {
            const auto target = a_actor->GetActorRuntimeData().currentCombatTarget.get();
            if (target) {
                targetPoint = target->GetPosition();
                targetPoint.z += static_cast<float>(target->GetHeight() * 0.6);
                hasTarget = true;
            }
        }

        const auto root = a_actor->GetCurrent3D();
        const auto node = root ? root->GetObjectByName(a_left ? "AnimObjectL" : "AnimObjectR") : nullptr;
        RE::NiPoint3 origin;
        RE::Projectile::ProjectileRot angles{};
        if (node && hasTarget) {
            origin = node->world * (a_left ? Forms::firePointLeft : Forms::firePointRight);
            const float dx = targetPoint.x - origin.x;
            const float dy = targetPoint.y - origin.y;
            const float dz = targetPoint.z - origin.z;
            angles.x = -std::atan2(dz, std::sqrt(dx * dx + dy * dy));
            angles.z = std::atan2(dx, dy);
        } else {
            origin = a_actor->GetPosition();
            origin.z += 96.0f;
            angles.x = a_actor->GetAimAngle();
            angles.z = a_actor->GetAimHeading();
        }

        if (ammo) {
            RE::Projectile::LaunchData launchData(a_actor, origin, angles, ammo, weapon);
            launchData.enchantItem = enchantment;
            launchData.poison = poison;
            RE::ProjectileHandle handle;
            firingLeft = a_left;
            RE::Projectile::Launch(&handle, launchData);
            firingLeft = false;
            if (poison) {
                DecrementPoisonCount(entry);
            }

            const auto spell = GetSpecialSpell(*variant, ammo);
            const auto effect = spell && !spell->effects.empty() ? spell->effects[0] : nullptr;
            const auto projectile = effect && effect->baseEffect ? effect->baseEffect->data.projectileBase : nullptr;
            if (projectile) {
                RE::Projectile::LaunchData spellData(a_actor, origin, angles, spell);
                spellData.autoAim = false;
                spellData.projectileBase = projectile;
                RE::ProjectileHandle spellHandle;
                RE::Projectile::Launch(&spellHandle, spellData);
            }

            a_actor->RemoveItem(ammo, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
        }

        Ammo::Look(a_actor, *variant);
        if (a_actor->GetCurrentAmmo()) {
            Ammo::ShowCounter(a_actor, State::Get(a_actor).ammo[variant->index]);
        } else {
            Ammo::HideCounter(a_actor);
        }
    }

    void OnWeaponDraw(RE::Actor* a_actor, bool a_draw)
    {
        std::scoped_lock guard(State::lock);
        auto& data = State::Get(a_actor);
        if (!a_draw) {
            data.aiming = { false, false };
            Ammo::HideCounter(a_actor);
            Fire::SetHoldDown(a_actor, true, 0.0f);
            Fire::SetHoldDown(a_actor, false, 0.0f);
            return;
        }
        for (const bool left : { false, true }) {
            const auto variant = Forms::GetVariant(Forms::GetHandWeapon(a_actor, left));
            if (variant) {
                Ammo::ShowCounter(a_actor, data.ammo[variant->index]);
            }
        }
    }

    enum class Graph {
        kFire,
        kDraw,
        kSheathe,
        kBowDrawn
    };

    struct GraphTask {
        RE::ActorHandle handle;
        Graph event;
        bool left;

        void operator()() const
        {
            const auto actor = handle.get();
            if (!actor) {
                return;
            }
            switch (event) {
            case Graph::kFire:
                FireCrossbow(actor.get(), left);
                break;
            case Graph::kDraw:
            case Graph::kSheathe:
                OnWeaponDraw(actor.get(), event == Graph::kDraw);
                break;
            case Graph::kBowDrawn:
                Ammo::CheckInvalidAmmo2H(actor.get());
                break;
            }
        }
    };

    void OnGraphEvent(const RE::BSAnimationGraphEvent* a_event)
    {
        static const RE::BSFixedString rightFire{ "MRh_SpellFire_Event" };
        static const RE::BSFixedString leftFire{ "MLh_SpellFire_Event" };
        static const RE::BSFixedString weaponDraw{ "weaponDraw" };
        static const RE::BSFixedString weaponSheathe{ "weaponSheathe" };
        static const RE::BSFixedString bowDrawn{ "BowDrawn" };

        const auto& tag = a_event->tag;
        Graph event;
        if (tag == rightFire || tag == leftFire) {
            event = Graph::kFire;
        } else if (tag == weaponDraw) {
            event = Graph::kDraw;
        } else if (tag == weaponSheathe) {
            event = Graph::kSheathe;
        } else if (tag == bowDrawn) {
            event = Graph::kBowDrawn;
        } else {
            return;
        }

        const auto actor = a_event->holder->As<RE::Actor>();
        if (!actor) {
            return;
        }

        const bool left = tag == leftFire;
        const bool relevant = event == Graph::kFire ? Forms::GetHandWeapon(actor, left) != nullptr : event == Graph::kBowDrawn || HoldsCrossbow(actor);
        if (relevant) {
            SKSE::GetTaskInterface()->AddTask(GraphTask{ actor->GetHandle(), event, left });
        }
    }

    // one per vtable, each keeps its own original in case another plugin hooked it first
    template <std::size_t N>
    struct GraphEventSink {
        static RE::BSEventNotifyControl thunk(RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_this, RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_source)
        {
            if (a_event && a_event->holder) {
                OnGraphEvent(a_event);
            }
            return func(a_this, a_event, a_source);
        }
        static inline REL::Relocation<decltype(thunk)> func;
    };

    // temper comes from the hand that fired, the engine always reads the right hand
    struct ProjectileTemper {
        static RE::InventoryEntryData* thunk(RE::AIProcess* a_process, bool a_left)
        {
            return func(a_process, a_left || firingLeft);
        }
        static inline REL::Relocation<decltype(thunk)> func;
    };
}

namespace Fire {
    void Install()
    {
        REL::Relocation<std::uintptr_t> character{ RE::VTABLE_Character[2] };
        REL::Relocation<std::uintptr_t> player{ RE::VTABLE_PlayerCharacter[2] };
        GraphEventSink<0>::func = character.write_vfunc(0x1, GraphEventSink<0>::thunk);  // BSTEventSink<BSAnimationGraphEvent>::ProcessEvent
        GraphEventSink<1>::func = player.write_vfunc(0x1, GraphEventSink<1>::thunk);

        const bool latest = REL::Module::get().version() >= SKSE::RUNTIME_SSE_1_7_104;
        auto& trampoline = SKSE::GetTrampoline();
        const auto constructor = REL::ID(44100).address();  // Projectile::Constructor
        ProjectileTemper::func = trampoline.write_call<5>(constructor + (latest ? 0x2CA : 0x2B1), ProjectileTemper::thunk);  // ActorProcess::GetEquippedWeapon
        trampoline.write_call<5>(constructor + (latest ? 0x2DD : 0x2C4), ProjectileTemper::thunk);
    }

    void StartLaunch(RE::Actor* a_actor, bool a_left)
    {
        if (!IsNotEquipping(a_actor)) {
            return;
        }
        const auto weapon = Forms::GetHandWeapon(a_actor, a_left);
        if (!weapon || !IsReadyToFire(a_actor)) {
            return;
        }
        const auto variant = Forms::GetVariant(weapon);

        std::scoped_lock guard(State::lock);
        Ammo::Assign(a_actor, *variant);
        auto& data = State::Get(a_actor);
        if (data.aiming[a_left] || Ammo::CheckForFailure(a_actor, *variant, a_left)) {
            return;
        }

        SetHoldDown(a_actor, a_left, 1.0f);
        a_actor->SetGraphVariableBool("NotCasting", false);
        if (a_left) {
            a_actor->NotifyAnimationGraph("BeginCastLeft");
            a_actor->NotifyAnimationGraph("MLh_SpellAimedStart");
            a_actor->NotifyAnimationGraph("MLh_PreChargeOut");
            a_actor->NotifyAnimationGraph("MLh_SpellReady_Event");
            a_actor->NotifyAnimationGraph("MLh_PreAimedOut");
        } else {
            a_actor->NotifyAnimationGraph("BeginCastRight");
            a_actor->NotifyAnimationGraph("MRh_SpellAimedStart");
            a_actor->NotifyAnimationGraph("MRh_SpellReady_Event");
        }
        data.aiming[a_left] = true;
    }

    void EndLaunch(RE::Actor* a_actor, bool a_left)
    {
        if (!IsNotEquipping(a_actor)) {
            return;
        }
        const auto weapon = Forms::GetHandWeapon(a_actor, a_left);
        if (!weapon || !IsReadyToFire(a_actor)) {
            return;
        }

        std::scoped_lock guard(State::lock);
        auto& data = State::Get(a_actor);
        if (data.aiming[a_left]) {
            a_actor->NotifyAnimationGraph(a_left ? "MLh_SpellRelease_Event" : "MRh_SpellRelease_Event");
            SetHoldDown(a_actor, a_left, 0.0f);
        } else {
            a_actor->SetGraphVariableBool("NotCasting", true);
            a_actor->NotifyAnimationGraph("CastStop");
        }
        data.aiming[a_left] = false;
    }

    void SetHoldDown(RE::Actor* a_actor, bool a_left, float a_value)
    {
        if (a_actor->IsPlayerRef()) {
            (a_left ? Forms::leftHoldDown : Forms::rightHoldDown)->value = a_value;
        }
    }
}
