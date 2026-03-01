#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "AudioEngine.h"
#include <spdlog/spdlog.h>
#include <mutex>
#include <cmath>

static ma_device g_device;
static std::mutex g_audioMutex;

// Playback state
static std::vector<Tone> g_currentSequence;
static size_t g_sequenceIndex = 0;
static float g_phase = 0.0f;
static int g_samplesRemainingInTone = 0;
static int g_lowBatTimerSamples = 0;

std::atomic<bool> AudioEngine::s_initialized{false};
std::atomic<bool> AudioEngine::s_lowBattery{false};

void audioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    float* fOut = (float*)pOutput;
    float sampleRate = pDevice->sampleRate;
    
    std::lock_guard<std::mutex> lock(g_audioMutex);

    for (unsigned int i = 0; i < frameCount; ++i) {
        float sample = 0.0f;

        // Sequence playing
        if (g_sequenceIndex < g_currentSequence.size()) {
            Tone currentTone = g_currentSequence[g_sequenceIndex];
            
            if (currentTone.frequency > 0.0f) {
                // Generate sine wave
                float phaseIncrement = (2.0f * 3.14159265359f * currentTone.frequency) / sampleRate;
                sample += std::sin(g_phase) * 0.2f; // Volume = 0.2
                g_phase += phaseIncrement;
                if (g_phase > 2.0f * 3.14159265359f) g_phase -= 2.0f * 3.14159265359f;
            }

            g_samplesRemainingInTone--;
            if (g_samplesRemainingInTone <= 0) {
                g_sequenceIndex++;
                if (g_sequenceIndex < g_currentSequence.size()) {
                    g_samplesRemainingInTone = (g_currentSequence[g_sequenceIndex].duration_ms * sampleRate) / 1000.0f;
                    g_phase = 0.0f;
                }
            }
        } 
        // If no sequence, check low battery alarm
        else if (AudioEngine::isLowBattery()) {
            g_lowBatTimerSamples++;
            // Beep for 200ms every 1000ms
            int totalCycle = sampleRate * 1.0f;
            int beepDuration = sampleRate * 0.2f;
            
            if (g_lowBatTimerSamples % totalCycle < beepDuration) {
                float freq = 880.0f; // High pitch A5
                float phaseIncrement = (2.0f * 3.14159265359f * freq) / sampleRate;
                sample += std::sin(g_phase) * 0.15f; 
                g_phase += phaseIncrement;
                if (g_phase > 2.0f * 3.14159265359f) g_phase -= 2.0f * 3.14159265359f;
            } else {
                g_phase = 0.0f;
            }
        }

        // Stereo output
        *fOut++ = sample;
        *fOut++ = sample;
    }
}

bool AudioEngine::init() {
    if (s_initialized) return true;

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = ma_format_f32;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate        = 44100;
    deviceConfig.dataCallback      = audioCallback;

    if (ma_device_init(NULL, &deviceConfig, &g_device) != MA_SUCCESS) {
        spdlog::error("Failed to initialize miniaudio device.");
        return false;
    }

    ma_device_start(&g_device);
    s_initialized = true;
    spdlog::info("AudioEngine initialized.");
    return true;
}

void AudioEngine::shutdown() {
    if (s_initialized) {
        ma_device_uninit(&g_device);
        s_initialized = false;
    }
}

void AudioEngine::playSequence(const std::vector<Tone>& sequence) {
    if (!s_initialized) return;
    
    std::lock_guard<std::mutex> lock(g_audioMutex);
    g_currentSequence = sequence;
    g_sequenceIndex = 0;
    g_phase = 0.0f;
    if (!sequence.empty()) {
        g_samplesRemainingInTone = (sequence[0].duration_ms * g_device.sampleRate) / 1000.0f;
    }
}

void AudioEngine::playConnect() {
    // Ascending major chord (C E G C)
    playSequence({
        {523.25f, 100}, // C5
        {0.0f, 20},     // silence
        {659.25f, 100}, // E5
        {0.0f, 20},
        {783.99f, 100}, // G5
        {0.0f, 20},
        {1046.50f, 200} // C6
    });
}

void AudioEngine::playDisconnect() {
    // Descending chord
    playSequence({
        {1046.50f, 100}, // C6
        {0.0f, 20},
        {783.99f, 100},  // G5
        {0.0f, 20},
        {659.25f, 100},  // E5
        {0.0f, 20},
        {523.25f, 200}   // C5
    });
}

void AudioEngine::playTakeoff() {
    // Ascending frequency sweep (simulated with 3 quick beeps)
    playSequence({
        {440.0f, 80},
        {0.0f, 10},
        {660.0f, 80},
        {0.0f, 10},
        {880.0f, 150}
    });
}

void AudioEngine::playLand() {
    // Descending frequency sweep
    playSequence({
        {880.0f, 80},
        {0.0f, 10},
        {660.0f, 80},
        {0.0f, 10},
        {440.0f, 150}
    });
}

void AudioEngine::setLowBattery(bool isLow) {
    s_lowBattery = isLow;
}
