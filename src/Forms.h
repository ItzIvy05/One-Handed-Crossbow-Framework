#pragma once

namespace Forms {
    struct Variant {
        std::size_t index{};
        RE::BGSKeyword* weaponKeyword{};
        RE::BGSKeyword* ammoKeyword{};
        RE::BGSListForm* specialAmmo{};
        RE::BGSListForm* specialSpells{};
        RE::BGSMessage* noAmmoMessage{};
        RE::TESSound* shootSound{};
    };

    inline Variant crossbow;
    inline Variant pistol;
    inline std::array<Variant*, 2> variants{ &crossbow, &pistol };

    inline RE::NiPoint3 firePointRight{};
    inline RE::NiPoint3 firePointLeft{};

    inline RE::TESGlobal* distance{};
    inline RE::TESGlobal* height{};
    inline RE::TESGlobal* npcsDropInvalidWeapons{};
    inline RE::TESGlobal* npcDistanceMin{};
    inline RE::TESGlobal* npcDistanceMax{};
    inline RE::TESGlobal* staminaCostEnabled{};
    inline RE::TESGlobal* staminaCostAmount{};
    inline RE::TESGlobal* leftHoldDown{};
    inline RE::TESGlobal* rightHoldDown{};

    inline RE::BGSMessage* noStamina{};
    inline RE::BGSMessage* notEnoughCharge{};
    inline RE::BGSMessage* noAmmo2HCrossbow{};
    inline RE::BGSMessage* noAmmo2HGun{};

    inline RE::TESFaction* currentFollowerFaction{};
    inline RE::TESFaction* customFollowerFaction{};

    inline RE::SpellItem* npcSlow{};
    inline RE::SpellItem* monitorCrossbow{};
    inline RE::SpellItem* monitorPistol{};
    inline float npcCooldown{};

    inline RE::BGSKeyword* ammoBullet1H{};
    inline RE::BGSKeyword* ammoBullet2H{};
    inline RE::BGSKeyword* gun2H{};
    inline RE::BGSKeyword* gun2HHandgun{};
    inline RE::BGSKeyword* gun2HRifle{};
    inline RE::BGSKeyword* gun2HGrenade{};
    inline RE::BGSKeyword* gun2HBlunderbuss{};
    inline RE::BGSKeyword* ammoBullet2HHandgun{};

    void ReadConfig();
    void Load();

    const Variant* GetVariant(const RE::TESForm* a_form);
    RE::TESObjectWEAP* GetHandWeapon(RE::Actor* a_actor, bool a_left);
    bool IsFollower(RE::Actor* a_actor);
    void ShowMessage(RE::BGSMessage* a_message);
}

namespace State {
    struct NPCCycle {
        std::uint32_t hand{};
        float release{};
    };

    // Aiming per Hand, Ammo per Variant and NPC Control Timer Tracker
    struct Data {
        std::array<bool, 2> aiming{};
        std::array<RE::TESAmmo*, 2> ammo{};
        std::array<NPCCycle, 2> cycle{};
        float poll{};
        float cooldown{};
    };

    inline std::recursive_mutex lock;
    inline std::unordered_map<RE::FormID, Data> actors;

    Data& Get(RE::Actor* a_actor);
    void Clear();
}
