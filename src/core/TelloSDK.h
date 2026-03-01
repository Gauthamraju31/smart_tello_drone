#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>

struct TelemetryData {
    int pitch = 0, roll = 0, yaw = 0;
    int vgx = 0, vgy = 0, vgz = 0;
    int templ = 0, temph = 0;
    int tof = 0, h = 0, bat = 0;
    float baro = 0.0f;
    int flightTime = 0;
    float agx = 0.0f, agy = 0.0f, agz = 0.0f;
    int64_t timestamp_ms = 0;
};

class TelloSDK {
public:
    enum class State { Disconnected, Connected, Streaming, Flying };

    TelloSDK();
    ~TelloSDK();

    // connection
    bool connect();
    void disconnect();

    // commands
    bool takeoff();
    bool land();
    bool emergency();
    bool moveForward(int cm);
    bool moveBack(int cm);
    bool moveLeft(int cm);
    bool moveRight(int cm);
    bool moveUp(int cm);
    bool moveDown(int cm);
    bool rotateCW(int deg);
    bool rotateCCW(int deg);
    bool flip(char direction);
    bool setSpeed(int cms);
    bool sendRC(int roll, int pitch, int throttle, int yaw);

    // video
    bool streamOn();
    bool streamOff();

    // query
    int queryBattery();

    // state access
    State getState() const { return m_state.load(); }
    TelemetryData getLatestTelemetry();

    // callbacks
    std::function<void(const uint8_t*, size_t)> onVideoData;
    std::function<void(const TelemetryData&)> onTelemetry;
    std::function<void(const std::string&)> onResponse; // raw response logging

private:
    bool sendCommand(const std::string& cmd, bool expectOk = true);
    std::string receiveResponse(int timeout_ms = 2000);

    void stateThreadFunc();
    void videoThreadFunc();
    void keepAliveThreadFunc();
    TelemetryData parseTelemetry(const std::string& data);

    int m_cmdSocket;
    int m_stateSocket;
    int m_videoSocket;

    std::atomic<State> m_state;
    std::atomic<bool> m_running;

    std::thread m_stateThread;
    std::thread m_videoThread;
    std::thread m_keepAliveThread;

    std::mutex m_telemetryMutex;
    TelemetryData m_latestTelemetry;

    const std::string TELLO_IP = "192.168.10.1";
    const int CMD_PORT = 8889;
    const int STATE_PORT = 8890;
    const int VIDEO_PORT = 11111;
};
