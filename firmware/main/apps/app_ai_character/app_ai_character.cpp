/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_ai_character.h"
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <assets/assets.h>
#include <smooth_lvgl.hpp>
#include <stackchan/stackchan.h>
#include <apps/common/common.h>

using namespace mooncake;
using namespace smooth_ui_toolkit::lvgl_cpp;
using namespace stackchan;

AppAiCharacter::AppAiCharacter()
{
    setAppInfo().name           = "AI.CHARACTER";
    static auto icon            = assets::get_image("icon_sentinel.bin");
    setAppInfo().icon           = (void*)&icon;
    static uint32_t theme_color = 0x9966FF;
    setAppInfo().userData       = (void*)&theme_color;
}

void AppAiCharacter::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "state: null -> created");
}

void AppAiCharacter::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "state: created -> go_open");

    LvglLockGuard lock;

    auto avatar = std::make_unique<avatar::DefaultAvatar>();
    avatar->init(lv_screen_active());
    GetStackChan().attachAvatar(std::move(avatar));

    GetStackChan().addModifier(std::make_unique<BlinkModifier>());
    GetStackChan().addModifier(std::make_unique<BreathModifier>());
    GetStackChan().addModifier(std::make_unique<IdleMotionModifier>());
    GetStackChan().addModifier(std::make_unique<IdleExpressionModifier>());

    view::create_home_indicator([&]() { close(); }, 0xC4A3FF, 0x3D1A6E);

    // Start the voice pipeline in its own FreeRTOS task
    _pipeline = std::make_unique<VoicePipeline>();
    _pipeline->start();
}

void AppAiCharacter::onRunning()
{
    if (!_entered_running) {
        _entered_running = true;
        mclog::tagInfo(getAppInfo().name, "state: go_open -> running");
    }

    // Sync avatar to pipeline state changes
    auto state = _pipeline->getState();
    if (state != _last_pipeline_state) {
        _last_pipeline_state = state;

        LvglLockGuard lock;
        switch (state) {
            case PipelineState::IDLE:
                mclog::tagInfo(getAppInfo().name, "pipeline -> idle");
                if (_speaking_modifier_id >= 0) {
                    GetStackChan().removeModifier(_speaking_modifier_id);
                    _speaking_modifier_id = -1;
                }
                break;

            case PipelineState::LISTENING:
                mclog::tagInfo(getAppInfo().name, "pipeline -> listening");
                // Nothing extra — idle animations keep running, which looks natural
                break;

            case PipelineState::PROCESSING:
                mclog::tagInfo(getAppInfo().name, "pipeline -> processing");
                if (_speaking_modifier_id >= 0) {
                    GetStackChan().removeModifier(_speaking_modifier_id);
                    _speaking_modifier_id = -1;
                }
                break;

            case PipelineState::SPEAKING:
                mclog::tagInfo(getAppInfo().name, "pipeline -> speaking");
                _speaking_modifier_id =
                    GetStackChan().addModifier(std::make_unique<SpeakingModifier>());
                break;
        }
    }

    {
        LvglLockGuard lock;
        GetStackChan().update();
        view::update_home_indicator();
    }
}

void AppAiCharacter::onClose()
{
    _entered_running = false;
    mclog::tagInfo(getAppInfo().name, "state: running -> go_close");

    // Stop pipeline before tearing down avatar
    if (_pipeline) {
        _pipeline->stop();
        _pipeline.reset();
    }

    LvglLockGuard lock;
    GetStackChan().resetAvatar();
    view::destroy_home_indicator();
}
