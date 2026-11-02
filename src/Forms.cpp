#include "Forms.h"

namespace {
    constexpr std::string_view plugin = "OneHand_Crossbow_Framework.esp"sv;
    constexpr std::string_view config = "Data/SKSE/Plugins/CrossbowFramework.ini"sv;

    std::string Trim(std::string_view a_string)
    {
        const auto start = a_string.find_first_not_of(" \t\r\n");
        if (start == std::string_view::npos) {
            return {};
        }
        const auto end = a_string.find_last_not_of(" \t\r\n");
        return std::string(a_string.substr(start, end - start + 1));
    }

    [[noreturn]] void Fail(const std::string& a_line)
    {
        SKSE::stl::report_and_fail(std::format("CrossbowFramework: bad line in {}: {}", config, a_line));
    }

    RE::NiPoint3 ParsePoint(const std::string& a_line, std::string_view a_value)
    {
        std::array<float, 3> values{};
        std::size_t start = 0;
        for (std::size_t i = 0; i < values.size(); ++i) {
            const auto end = i + 1 < values.size() ? a_value.find(',', start) : a_value.size();
            if (end == std::string_view::npos) {
                Fail(a_line);
            }
            const auto part = Trim(a_value.substr(start, end - start));
            const auto result = std::from_chars(part.data(), part.data() + part.size(), values[i]);
            if (part.empty() || result.ec != std::errc() || result.ptr != part.data() + part.size()) {
                Fail(a_line);
            }
            start = end + 1;
        }
        return { values[0], values[1], values[2] };
    }

    template <class T>
    T* Lookup(RE::FormID a_id, std::string_view a_plugin = plugin)
    {
        const auto form = RE::TESDataHandler::GetSingleton()->LookupForm<T>(a_id, a_plugin);
        if (!form) {
            SKSE::stl::report_and_fail(std::format("CrossbowFramework: form {:X} is missing from {}", a_id, a_plugin));
        }
        return form;
    }

    Forms::Variant MakeVariant(std::size_t a_index, RE::FormID a_weapon, RE::FormID a_ammo, RE::FormID a_specialAmmo, RE::FormID a_specialSpells, RE::FormID a_noAmmo, RE::FormID a_sound)
    {
        Forms::Variant variant;
        variant.index = a_index;
        variant.weaponKeyword = Lookup<RE::BGSKeyword>(a_weapon);
        variant.ammoKeyword = Lookup<RE::BGSKeyword>(a_ammo);
        variant.specialAmmo = Lookup<RE::BGSListForm>(a_specialAmmo);
        variant.specialSpells = Lookup<RE::BGSListForm>(a_specialSpells);
        variant.noAmmoMessage = Lookup<RE::BGSMessage>(a_noAmmo);
        variant.shootSound = Lookup<RE::TESSound>(a_sound);
        return variant;
    }
}

namespace Forms {
    void ReadConfig()
    {
        std::ifstream stream{ std::filesystem::path(config) };
        if (!stream) {
            SKSE::stl::report_and_fail(std::format("CrossbowFramework: {} is missing", config));
        }

        bool rightSet = false;
        bool leftSet = false;
        std::string line;
        while (std::getline(stream, line)) {
            const auto text = Trim(line);
            if (text.empty() || text.starts_with(';') || text.starts_with('[')) {
                continue;
            }
            const auto equals = text.find('=');
            if (equals == std::string::npos) {
                Fail(text);
            }
            const auto key = Trim(std::string_view(text).substr(0, equals));
            const auto value = Trim(std::string_view(text).substr(equals + 1));
            if (key == "FirePointRight") {
                firePointRight = ParsePoint(text, value);
                rightSet = true;
            } else if (key == "FirePointLeft") {
                firePointLeft = ParsePoint(text, value);
                leftSet = true;
            } else {
                Fail(text);
            }
        }

        if (!rightSet || !leftSet) {
            SKSE::stl::report_and_fail(std::format("CrossbowFramework: FirePointRight and FirePointLeft must be set in {}", config));
        }
    }

    void Load()
    {
        crossbow = MakeVariant(0, 0x8CE, 0x8CB, 0x809, 0x80A, 0xCCA, 0x83F);
        pistol = MakeVariant(1, 0x80E, 0x812, 0x813, 0x818, 0x817, 0x840);

        distance = Lookup<RE::TESGlobal>(0x80C);
        height = Lookup<RE::TESGlobal>(0x80D);
        npcsDropInvalidWeapons = Lookup<RE::TESGlobal>(0x8C6);
        npcDistanceMin = Lookup<RE::TESGlobal>(0x8DC);
        npcDistanceMax = Lookup<RE::TESGlobal>(0x8DD);
        staminaCostEnabled = Lookup<RE::TESGlobal>(0xFE8);
        staminaCostAmount = Lookup<RE::TESGlobal>(0xFE9);
        leftHoldDown = Lookup<RE::TESGlobal>(0xFED);
        rightHoldDown = Lookup<RE::TESGlobal>(0xFEE);

        noStamina = Lookup<RE::BGSMessage>(0xFEA);
        notEnoughCharge = Lookup<RE::BGSMessage>(0x953);
        noAmmo2HCrossbow = Lookup<RE::BGSMessage>(0x81B);
        noAmmo2HGun = Lookup<RE::BGSMessage>(0x81C);

        currentFollowerFaction = Lookup<RE::TESFaction>(0x5C84E, "Skyrim.esm"sv);
        customFollowerFaction = Lookup<RE::TESFaction>(0x8C7);

        npcSlow = Lookup<RE::SpellItem>(0x8E4);
        monitorCrossbow = Lookup<RE::SpellItem>(0x9C3);
        monitorPistol = Lookup<RE::SpellItem>(0x825);
        const auto cooldown = Lookup<RE::SpellItem>(0x8E2);
        npcCooldown = static_cast<float>(cooldown->effects[0]->effectItem.duration);

        ammoBullet1H = Lookup<RE::BGSKeyword>(0x812);
        ammoBullet2H = Lookup<RE::BGSKeyword>(0x819);
        gun2H = Lookup<RE::BGSKeyword>(0x81A);
        gun2HHandgun = Lookup<RE::BGSKeyword>(0x836);
        gun2HRifle = Lookup<RE::BGSKeyword>(0x835);
        gun2HGrenade = Lookup<RE::BGSKeyword>(0x838);
        gun2HBlunderbuss = Lookup<RE::BGSKeyword>(0x837);
        ammoBullet2HHandgun = Lookup<RE::BGSKeyword>(0x832);
    }

    const Variant* GetVariant(const RE::TESForm* a_form)
    {
        const auto weapon = a_form ? a_form->As<RE::TESObjectWEAP>() : nullptr;
        if (!weapon) {
            return nullptr;
        }
        if (crossbow.weaponKeyword && weapon->HasKeyword(crossbow.weaponKeyword)) {
            return &crossbow;
        }
        if (pistol.weaponKeyword && weapon->HasKeyword(pistol.weaponKeyword)) {
            return &pistol;
        }
        return nullptr;
    }

    RE::TESObjectWEAP* GetHandWeapon(RE::Actor* a_actor, bool a_left)
    {
        const auto object = a_actor ? a_actor->GetEquippedObject(a_left) : nullptr;
        const auto weapon = object ? object->As<RE::TESObjectWEAP>() : nullptr;
        return GetVariant(weapon) ? weapon : nullptr;
    }

    bool IsFollower(RE::Actor* a_actor)
    {
        return a_actor->IsInFaction(currentFollowerFaction) || a_actor->IsInFaction(customFollowerFaction);
    }

    void ShowMessage(RE::BGSMessage* a_message)
    {
        RE::BSString text;
        a_message->GetDescription(text, a_message);
        RE::SendHUDMessage::ShowHUDMessage(text.c_str());
    }
}

namespace State {
    Data& Get(RE::Actor* a_actor)
    {
        return actors[a_actor->GetFormID()];
    }

    void Clear()
    {
        std::scoped_lock guard(lock);
        actors.clear();
    }
}
