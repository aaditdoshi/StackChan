/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "voice_pipeline.h"
#include "config.h"
#include "wav_utils.h"
#include <hal/hal.h>
#include <mooncake_log.h>
#include <algorithm>
#include <cmath>
#include <esp_http_client.h>

static const std::string_view _tag = "VoicePipeline";

// ── Task glue ─────────────────────────────────────────────────────────────────

void VoicePipeline::taskEntry(void* arg)
{
    static_cast<VoicePipeline*>(arg)->taskLoop();
    vTaskDelete(nullptr);
}

void VoicePipeline::start()
{
    if (_running.exchange(true))
        return;
    xTaskCreate(taskEntry, "voice_pipeline", 12 * 1024, this, 5, &_task_handle);
}

void VoicePipeline::stop()
{
    _running = false;
    if (_task_handle) {
        // Give the task up to 35 s to finish its current HTTP call then exit
        for (int i = 0; i < 3500 && eTaskGetState(_task_handle) != eDeleted; ++i)
            vTaskDelay(pdMS_TO_TICKS(10));
        _task_handle = nullptr;
    }
    // Make sure audio hardware is released
    GetHAL().stopVoiceCapture();
    GetHAL().stopVoicePlayback();
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void VoicePipeline::taskLoop()
{
    mclog::tagInfo(_tag, "task started");

    while (_running) {
        _state = PipelineState::IDLE;

        // ── 1. Record until silence ───────────────────────────────────────────
        _state = PipelineState::LISTENING;
        mclog::tagInfo(_tag, "state: idle -> listening");

        std::vector<int16_t> recording;
        if (!recordUntilSilence(recording)) {
            // No voice detected or task was stopped
            continue;
        }

        // ── 2. Send to pipeline server ────────────────────────────────────────
        _state = PipelineState::PROCESSING;
        mclog::tagInfo(_tag, "state: listening -> processing (%u samples)", recording.size());

        std::string wav     = buildWav(recording, GetHAL().getAudioSampleRate());
        std::string response = postAudio(wav);

        if (response.empty()) {
            mclog::tagError(_tag, "pipeline server returned no audio");
            continue;
        }

        // ── 3. Play back TTS response ─────────────────────────────────────────
        _state = PipelineState::SPEAKING;
        mclog::tagInfo(_tag, "state: processing -> speaking (%u bytes)", response.size());

        playWav(response);

        mclog::tagInfo(_tag, "state: speaking -> idle");
    }

    mclog::tagInfo(_tag, "task exiting");
}

// ── Recording with VAD ────────────────────────────────────────────────────────

bool VoicePipeline::recordUntilSilence(std::vector<int16_t>& out)
{
    const size_t   CHUNK_FRAMES   = 512;
    const uint32_t SAMPLE_RATE    = GetHAL().getAudioSampleRate();
    const uint32_t MS_PER_CHUNK   = (CHUNK_FRAMES * 1000u) / SAMPLE_RATE;  // ~21 ms
    const uint32_t SILENCE_CHUNKS = AI_CHAR_SILENCE_MS / MS_PER_CHUNK;

    GetHAL().startVoiceCapture();

    bool     speaking_started = false;
    uint32_t silence_count    = 0;
    std::vector<int16_t> chunk;

    while (_running) {
        if (!GetHAL().readVoiceChunk(chunk, CHUNK_FRAMES))
            break;

        // RMS energy
        float sum_sq = 0.0f;
        for (auto s : chunk)
            sum_sq += static_cast<float>(s) * s;
        float rms = sqrtf(sum_sq / chunk.size());

        if (rms > AI_CHAR_VAD_THRESHOLD) {
            speaking_started = true;
            silence_count    = 0;
        } else if (speaking_started) {
            ++silence_count;
        }

        if (speaking_started) {
            out.insert(out.end(), chunk.begin(), chunk.end());
            if (silence_count >= SILENCE_CHUNKS)
                break;
        }
    }

    GetHAL().stopVoiceCapture();
    return speaking_started && !out.empty();
}

// ── HTTP POST ─────────────────────────────────────────────────────────────────

std::string VoicePipeline::postAudio(const std::string& wav_data)
{
    esp_http_client_config_t cfg = {};
    cfg.url        = AI_CHAR_PIPELINE_URL;
    cfg.timeout_ms = AI_CHAR_HTTP_TIMEOUT_MS;

    auto client = esp_http_client_init(&cfg);
    if (!client) {
        mclog::tagError(_tag, "failed to init http client");
        return {};
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "audio/wav");

    esp_err_t err = esp_http_client_open(client, static_cast<int>(wav_data.size()));
    if (err != ESP_OK) {
        mclog::tagError(_tag, "http open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return {};
    }

    int written = esp_http_client_write(client, wav_data.data(),
                                        static_cast<int>(wav_data.size()));
    if (written < 0) {
        mclog::tagError(_tag, "http write failed");
        esp_http_client_cleanup(client);
        return {};
    }

    int content_len = esp_http_client_fetch_headers(client);
    int status      = esp_http_client_get_status_code(client);

    if (status != 200 || content_len <= 0) {
        mclog::tagError(_tag, "pipeline returned status %d, content_len %d", status, content_len);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return {};
    }

    std::string response(content_len, '\0');
    int read = esp_http_client_read_response(client, &response[0], content_len);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (read != content_len) {
        mclog::tagError(_tag, "short read: got %d of %d bytes", read, content_len);
        return {};
    }

    return response;
}

// ── Playback ──────────────────────────────────────────────────────────────────

void VoicePipeline::playWav(const std::string& wav_data)
{
    size_t offset = wavDataOffset(wav_data);
    if (offset >= wav_data.size()) {
        mclog::tagError(_tag, "invalid WAV from server");
        return;
    }

    const int16_t* pcm        = reinterpret_cast<const int16_t*>(wav_data.data() + offset);
    const size_t   num_samples = (wav_data.size() - offset) / sizeof(int16_t);
    const size_t   CHUNK       = 512;

    GetHAL().startVoicePlayback();

    std::vector<int16_t> chunk;
    size_t pos = 0;
    while (pos < num_samples && _running) {
        size_t n = std::min(CHUNK, num_samples - pos);
        chunk.assign(pcm + pos, pcm + pos + n);
        GetHAL().writeVoiceChunk(chunk);
        pos += n;
    }

    GetHAL().stopVoicePlayback();
}
