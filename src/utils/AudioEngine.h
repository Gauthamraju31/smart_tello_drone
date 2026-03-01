#pragma once

#include <vector>
#include <atomic>

// Simple struct to define a musical tone
struct Tone {
    float frequency;   // Hz
    int duration_ms;   // milliseconds
};

class AudioEngine {
public:
    static bool init();
    static void shutdown();
    
    // Fire-and-forget sound events
    static void playConnect();
    static void playDisconnect();
    static void playTakeoff();
    static void playLand();
    
    // Continuous state
    static void setLowBattery(bool isLow);
    static bool isLowBattery() { return s_lowBattery.load(); }

private:
    static void playSequence(const std::vector<Tone>& sequence);

    static std::atomic<bool> s_initialized;
    static std::atomic<bool> s_lowBattery;
};
