/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

enum class PipelineState {
    IDLE,
    LISTENING,
    PROCESSING,
    SPEAKING,
};

class VoicePipeline {
public:
    VoicePipeline()  = default;
    ~VoicePipeline() { stop(); }

    void start();
    void stop();

    PipelineState getState() const { return _state.load(); }

private:
    std::atomic<PipelineState> _state{PipelineState::IDLE};
    std::atomic<bool>          _running{false};
    TaskHandle_t               _task_handle{nullptr};

    static void taskEntry(void* arg);
    void        taskLoop();

    // Returns true when voice was detected and recording captured
    bool recordUntilSilence(std::vector<int16_t>& out);

    // POST WAV bytes to pipeline server, returns WAV response bytes (empty on error)
    std::string postAudio(const std::string& wav_data);

    // Play a WAV response through the speaker
    void playWav(const std::string& wav_data);
};
