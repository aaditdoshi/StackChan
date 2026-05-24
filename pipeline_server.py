"""
StackChan Local AI Pipeline Server
------------------------------------
Receives raw WAV audio from StackChan, runs it through:
  1. WhisperX  -> transcript text
  2. Ollama    -> LLM response text
  3. Piper TTS -> speech audio

Returns a WAV file at 24000 Hz mono PCM that StackChan plays back.

Requirements:
    pip install fastapi uvicorn python-multipart whisperx requests piper-tts numpy scipy

Download a Piper voice model:
    python -m piper.download en_US-lessac-medium
    (models land in ~/.local/share/piper by default)

Run:
    uvicorn pipeline_server:app --host 0.0.0.0 --port 8765
"""

import io
import wave
import tempfile
import os
import logging

import numpy as np
import requests
from scipy import signal as scipy_signal
from fastapi import FastAPI, Request
from fastapi.responses import Response

# ── Config ────────────────────────────────────────────────────────────────────

OLLAMA_URL   = "http://localhost:11434/api/chat"
OLLAMA_MODEL = "gemma3:4b"          # change to your model, e.g. "qwen2.5:3b"
SYSTEM_PROMPT = "You are a helpful assistant. Keep responses short and conversational."

PIPER_MODEL  = "en_US-lessac-medium"   # run: python -m piper.download en_US-lessac-medium
WHISPER_MODEL = "base"                 # tiny / base / small / medium
WHISPER_DEVICE = "cuda"               # or "cpu"

STACKCHAN_SAMPLE_RATE = 24000          # must match AUDIO_OUTPUT_SAMPLE_RATE in firmware

# ── Startup ───────────────────────────────────────────────────────────────────

logging.basicConfig(level=logging.INFO)
log = logging.getLogger("pipeline")

log.info("Loading WhisperX model '%s' on %s ...", WHISPER_MODEL, WHISPER_DEVICE)
import whisperx
_whisper = whisperx.load_model(WHISPER_MODEL, device=WHISPER_DEVICE)
log.info("WhisperX ready.")

log.info("Loading Piper voice '%s' ...", PIPER_MODEL)
from piper.voice import PiperVoice
_piper = PiperVoice.load(PIPER_MODEL)
log.info("Piper ready (native sample rate: %d Hz).", _piper.config.sample_rate)

# Conversation history (in-memory, resets on server restart)
_history: list[dict] = [{"role": "system", "content": SYSTEM_PROMPT}]

app = FastAPI()

# ── Helpers ───────────────────────────────────────────────────────────────────

def wav_bytes_to_numpy(wav_bytes: bytes) -> tuple[np.ndarray, int]:
    """Read raw WAV bytes -> (float32 array normalised to [-1,1], sample_rate)."""
    buf = io.BytesIO(wav_bytes)
    with wave.open(buf, "rb") as wf:
        sr = wf.getframerate()
        n_frames = wf.getnframes()
        raw = wf.readframes(n_frames)
    audio = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0
    return audio, sr


def numpy_to_wav_bytes(audio: np.ndarray, sample_rate: int) -> bytes:
    """Convert float32 numpy array -> WAV bytes at the given sample rate."""
    pcm = (np.clip(audio, -1.0, 1.0) * 32767).astype(np.int16)
    buf = io.BytesIO()
    with wave.open(buf, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        wf.writeframes(pcm.tobytes())
    return buf.getvalue()


def resample(audio: np.ndarray, from_sr: int, to_sr: int) -> np.ndarray:
    if from_sr == to_sr:
        return audio
    return scipy_signal.resample_poly(audio, to_sr, from_sr).astype(np.float32)


def transcribe(wav_bytes: bytes) -> str:
    audio, sr = wav_bytes_to_numpy(wav_bytes)
    # WhisperX expects float32 at 16 kHz
    audio_16k = resample(audio, sr, 16000)
    result = _whisper.transcribe(audio_16k, batch_size=8)
    text = " ".join(s["text"].strip() for s in result.get("segments", []))
    log.info("STT: %s", text)
    return text


def chat(user_text: str) -> str:
    _history.append({"role": "user", "content": user_text})
    resp = requests.post(
        OLLAMA_URL,
        json={"model": OLLAMA_MODEL, "messages": _history, "stream": False},
        timeout=60,
    )
    resp.raise_for_status()
    reply = resp.json()["message"]["content"].strip()
    _history.append({"role": "assistant", "content": reply})
    log.info("LLM: %s", reply)
    return reply


def synthesize(text: str) -> bytes:
    """Piper TTS -> WAV bytes resampled to STACKCHAN_SAMPLE_RATE."""
    buf = io.BytesIO()
    with wave.open(buf, "wb") as wf:
        _piper.synthesize(text, wf)
    raw_wav = buf.getvalue()

    audio, native_sr = wav_bytes_to_numpy(raw_wav)
    resampled = resample(audio, native_sr, STACKCHAN_SAMPLE_RATE)
    return numpy_to_wav_bytes(resampled, STACKCHAN_SAMPLE_RATE)

# ── Endpoint ──────────────────────────────────────────────────────────────────

@app.post("/process")
async def process(request: Request) -> Response:
    wav_bytes = await request.body()
    if not wav_bytes:
        return Response(status_code=400)

    try:
        transcript = transcribe(wav_bytes)
        if not transcript.strip():
            log.warning("Empty transcript, skipping LLM call.")
            return Response(status_code=204)

        reply_text = chat(transcript)
        reply_wav  = synthesize(reply_text)

        return Response(content=reply_wav, media_type="audio/wav")

    except Exception as e:
        log.exception("Pipeline error: %s", e)
        return Response(status_code=500)


@app.post("/transcribe")
async def transcribe_only(request: Request) -> dict:
    """Test endpoint — STT only, returns transcript as JSON text."""
    wav_bytes = await request.body()
    if not wav_bytes:
        return {"error": "no audio"}
    text = transcribe(wav_bytes)
    return {"text": text}


@app.delete("/history")
async def clear_history() -> dict:
    """Reset conversation history (curl -X DELETE http://localhost:8765/history)."""
    global _history
    _history = [{"role": "system", "content": SYSTEM_PROMPT}]
    log.info("Conversation history cleared.")
    return {"status": "cleared"}


@app.get("/health")
async def health() -> dict:
    return {"status": "ok", "model": OLLAMA_MODEL, "voice": PIPER_MODEL}
