/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// Build a standard 44-byte PCM WAV from a mono int16 sample buffer.
inline std::string buildWav(const std::vector<int16_t>& pcm, uint32_t sampleRate)
{
    uint32_t dataSize = static_cast<uint32_t>(pcm.size()) * sizeof(int16_t);
    uint32_t fileSize = dataSize + 36;
    std::string wav(44 + dataSize, '\0');

    auto w16 = [&](size_t off, uint16_t v) { memcpy(&wav[off], &v, 2); };
    auto w32 = [&](size_t off, uint32_t v) { memcpy(&wav[off], &v, 4); };

    memcpy(&wav[0],  "RIFF", 4);  w32(4,  fileSize);
    memcpy(&wav[8],  "WAVE", 4);
    memcpy(&wav[12], "fmt ", 4);  w32(16, 16);
    w16(20, 1);                   // PCM
    w16(22, 1);                   // mono
    w32(24, sampleRate);
    w32(28, sampleRate * 2);      // byte rate (1 ch * 2 bytes)
    w16(32, 2);                   // block align
    w16(34, 16);                  // bits per sample
    memcpy(&wav[36], "data", 4);  w32(40, dataSize);
    memcpy(&wav[44], pcm.data(), dataSize);
    return wav;
}

// Return the byte offset where PCM data starts in a WAV buffer.
// Scans for the "data" chunk rather than assuming a fixed 44-byte header.
inline size_t wavDataOffset(const std::string& wav)
{
    if (wav.size() < 44) return 0;
    // Scan for "data" chunk marker after the mandatory 12-byte RIFF header.
    for (size_t i = 12; i + 8 <= wav.size(); ++i) {
        if (wav[i] == 'd' && wav[i+1] == 'a' && wav[i+2] == 't' && wav[i+3] == 'a') {
            return i + 8;  // skip "data" + 4-byte size field
        }
    }
    return 44;  // fallback
}
