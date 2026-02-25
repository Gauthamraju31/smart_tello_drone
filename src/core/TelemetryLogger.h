#pragma once

#include "TelloSDK.h"
#include <string>
#include <fstream>
#include <mutex>
#include <vector>
#include <deque>

class TelemetryLogger {
public:
    TelemetryLogger();
    ~TelemetryLogger();

    // Start logging to a new CSV file in logs/
    bool start();
    void stop();

    // Call this from the TelloSDK::onTelemetry callback
    void log(const TelemetryData& data);

    // Get the last N samples for plotting (thread-safe)
    std::vector<TelemetryData> getHistory(size_t maxSamples = 100);

private:
    std::string generateFilename() const;

    std::ofstream m_csvFile;
    std::mutex m_mutex;
    bool m_isLogging;

    // Ring buffer for GUI plots
    std::deque<TelemetryData> m_history;
    const size_t MAX_HISTORY = 1000;
};
