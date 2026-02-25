#include "TelemetryLogger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <filesystem>

TelemetryLogger::TelemetryLogger() : m_isLogging(false) {}

TelemetryLogger::~TelemetryLogger() {
    stop();
}

std::string TelemetryLogger::generateFilename() const {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << "logs/telemetry_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".csv";
    return oss.str();
}

bool TelemetryLogger::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_isLogging) return true;

    if (!std::filesystem::exists("logs")) {
        std::filesystem::create_directory("logs");
    }

    std::string filename = generateFilename();
    m_csvFile.open(filename);
    if (!m_csvFile.is_open()) {
        std::cerr << "Failed to open " << filename << " for logging" << std::endl;
        return false;
    }

    // Write CSV header
    m_csvFile << "Timestamp_ms,Pitch,Roll,Yaw,VelX,VelY,VelZ,AccelX,AccelY,AccelZ,"
              << "TempLow,TempHigh,ToF,Height,Battery,Barometer,FlightTime\n";
    
    m_isLogging = true;
    m_history.clear();
    std::cout << "Started telemetry logging to " << filename << std::endl;
    return true;
}

void TelemetryLogger::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isLogging) return;
    
    if (m_csvFile.is_open()) {
        m_csvFile.close();
    }
    m_isLogging = false;
    std::cout << "Stopped telemetry logging" << std::endl;
}

void TelemetryLogger::log(const TelemetryData& data) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Add to history for GUI
    m_history.push_back(data);
    if (m_history.size() > MAX_HISTORY) {
        m_history.pop_front();
    }

    // Write to CSV
    if (m_isLogging && m_csvFile.is_open()) {
        m_csvFile << data.timestamp_ms << ","
                  << data.pitch << "," << data.roll << "," << data.yaw << ","
                  << data.vgx << "," << data.vgy << "," << data.vgz << ","
                  << data.agx << "," << data.agy << "," << data.agz << ","
                  << data.templ << "," << data.temph << ","
                  << data.tof << "," << data.h << "," << data.bat << ","
                  << data.baro << "," << data.flightTime << "\n";
    }
}

std::vector<TelemetryData> TelemetryLogger::getHistory(size_t maxSamples) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<TelemetryData> result;
    size_t count = std::min(m_history.size(), maxSamples);
    if (count == 0) return result;
    
    result.reserve(count);
    auto it = m_history.end() - count;
    for (; it != m_history.end(); ++it) {
        result.push_back(*it);
    }
    return result;
}

TelemetryData TelemetryLogger::getLatest() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_history.empty()) return TelemetryData{0};
    return m_history.back();
}
