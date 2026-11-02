#pragma once

namespace Fire {
    void Install();
    void StartLaunch(RE::Actor* a_actor, bool a_left);
    void EndLaunch(RE::Actor* a_actor, bool a_left);
    void SetHoldDown(RE::Actor* a_actor, bool a_left, float a_value);
}
