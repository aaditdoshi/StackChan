/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <mooncake.h>
#include <memory>
#include "voice_pipeline.h"

class AppAiCharacter : public mooncake::AppAbility {
public:
    AppAiCharacter();

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    std::unique_ptr<VoicePipeline> _pipeline;
    PipelineState _last_pipeline_state{PipelineState::IDLE};
    bool          _entered_running{false};
    int           _speaking_modifier_id{-1};
};
