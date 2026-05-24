/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "../modifiable.h"
#include "../utils/random.h"
#include <smooth_ui_toolkit.hpp>
#include <hal/hal.h>
#include <cstdint>

namespace stackchan {

/**
 * @brief Makes the avatar look like it's thinking: eyes drift upward and side-to-side,
 *        Doubt emotion is set, mouth is slightly pursed, head tilts up.
 *        Restores prior state when destroyed.
 */
class ThinkingModifier : public Modifier {
public:
    /**
     * @param destroyAfterMs How long to think (0 = permanent until removed)
     */
    explicit ThinkingModifier(uint32_t destroyAfterMs = 0)
    {
        uint32_t now = GetHAL().millis();
        if (destroyAfterMs > 0) {
            _destroy_at   = now + destroyAfterMs;
            _has_lifetime = true;
        }
        _phase_tick = now + 600;
    }

    void _update(Modifiable& stackchan) override
    {
        if (!stackchan.hasAvatar()) {
            return;
        }

        uint32_t now = GetHAL().millis();

        if (!_is_started) {
            _is_started   = true;
            _prev_emotion = stackchan.avatar().getEmotion();
            enter_thinking(stackchan);
            return;
        }

        if (_has_lifetime && now >= _destroy_at) {
            exit_thinking(stackchan);
            requestDestroy();
            return;
        }

        if (now >= _phase_tick) {
            advance_phase(stackchan);
        }
    }

private:
    enum class Phase { LookUpLeft, LookUpRight, LookUp };

    void enter_thinking(Modifiable& stackchan)
    {
        auto& avatar = stackchan.avatar();
        avatar.setEmotion(avatar::Emotion::Doubt);

        // Eyes drift up-left — classic "searching memory" gaze
        avatar.leftEye().setPosition({-15, -22});
        avatar.rightEye().setPosition({-15, -22});

        // Left eyebrow raised higher (one-brow skeptical look), right stays near neutral
        avatar.leftEyebrow().setPosition({-5, -20});
        avatar.rightEyebrow().setPosition({0, -5});

        // Mouth slightly pursed and tilted
        avatar.mouth().setWeight(8);
        avatar.mouth().setRotation(15);

        // Head tilts slightly up and to the left
        if (!stackchan.motion().isModifyLocked()) {
            stackchan.motion().lookAtNormalized(-0.15f, -0.30f, 140);
        }

        _phase      = Phase::LookUpLeft;
        _phase_tick = GetHAL().millis() + 2400;
    }

    void advance_phase(Modifiable& stackchan)
    {
        auto& avatar = stackchan.avatar();
        uint32_t now = GetHAL().millis();

        if (avatar.isModifyLocked()) {
            _phase_tick = now + 200;
            return;
        }

        switch (_phase) {
            case Phase::LookUpLeft:
                // Shift gaze up-right; right eyebrow raises to match
                avatar.leftEye().setPosition({16, -20});
                avatar.rightEye().setPosition({16, -20});
                avatar.leftEyebrow().setPosition({5, -5});
                avatar.rightEyebrow().setPosition({5, -20});
                avatar.mouth().setWeight(5);
                avatar.mouth().setRotation(3580);  // slight tilt other way
                if (!stackchan.motion().isModifyLocked()) {
                    stackchan.motion().lookAtNormalized(0.15f, -0.25f, 120);
                }
                _phase      = Phase::LookUpRight;
                _phase_tick = now + 2000;
                break;

            case Phase::LookUpRight:
                // Gaze drifts straight up — both eyebrows furrow slightly (deep ponder)
                avatar.leftEye().setPosition({0, -26});
                avatar.rightEye().setPosition({0, -26});
                avatar.leftEyebrow().setPosition({0, -12});
                avatar.rightEyebrow().setPosition({0, -12});
                avatar.mouth().setWeight(10);
                avatar.mouth().setRotation(Random::getInstance().getInt(10, 25));
                if (!stackchan.motion().isModifyLocked()) {
                    stackchan.motion().lookAtNormalized(0.0f, -0.35f, 100);
                }
                _phase      = Phase::LookUp;
                _phase_tick = now + 1800;
                break;

            case Phase::LookUp:
                // Return to up-left; asymmetric brows resume
                avatar.leftEye().setPosition({-15, -22});
                avatar.rightEye().setPosition({-15, -22});
                avatar.leftEyebrow().setPosition({-5, -20});
                avatar.rightEyebrow().setPosition({0, -5});
                avatar.mouth().setWeight(8);
                avatar.mouth().setRotation(15);
                if (!stackchan.motion().isModifyLocked()) {
                    stackchan.motion().lookAtNormalized(-0.15f, -0.30f, 130);
                }
                _phase      = Phase::LookUpLeft;
                _phase_tick = now + 2400;
                break;
        }
    }

    void exit_thinking(Modifiable& stackchan)
    {
        auto& avatar = stackchan.avatar();
        avatar.setEmotion(_prev_emotion);
        avatar.leftEye().setPosition({0, 0});
        avatar.rightEye().setPosition({0, 0});
        avatar.mouth().setWeight(0);
        avatar.mouth().setRotation(0);
        avatar.leftEyebrow().setPosition({0, 0});
        avatar.rightEyebrow().setPosition({0, 0});
    }

    Phase _phase           = Phase::LookUpLeft;
    uint32_t _phase_tick   = 0;
    uint32_t _destroy_at   = 0;
    bool _has_lifetime     = false;
    bool _is_started       = false;
    avatar::Emotion _prev_emotion = avatar::Emotion::Neutral;
};

}  // namespace stackchan
