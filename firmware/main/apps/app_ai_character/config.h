/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

// ── Edit these before building ────────────────────────────────────────────────

// IP address of the PC running pipeline_server.py
#define AI_CHAR_PIPELINE_HOST "192.168.1.100"

// Port matching --port in: uvicorn pipeline_server:app --port 8765
#define AI_CHAR_PIPELINE_PORT 8765

// Full URL (constructed from above — no need to edit this line)
#define AI_CHAR_PIPELINE_URL  "http://" AI_CHAR_PIPELINE_HOST ":" STR(AI_CHAR_PIPELINE_PORT) "/process"

// VAD: RMS energy above this threshold counts as voice (int16 range 0-32767)
#define AI_CHAR_VAD_THRESHOLD  500.0f

// Milliseconds of silence before recording stops
#define AI_CHAR_SILENCE_MS     1500

// HTTP timeout for the pipeline call (ms) — LLM inference can be slow
#define AI_CHAR_HTTP_TIMEOUT_MS 30000

// ── Internal helpers ──────────────────────────────────────────────────────────
#define _STR(x) #x
#define STR(x)  _STR(x)
