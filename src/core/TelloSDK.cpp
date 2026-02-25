#include "TelloSDK.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

namespace {
    int64_t now_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
}

TelloSDK::TelloSDK() : m_cmdSocket(-1), m_stateSocket(-1), m_videoSocket(-1), m_state(State::Disconnected), m_running(false) {
}

TelloSDK::~TelloSDK() {
    disconnect();
}

bool TelloSDK::connect() {
    if (m_state != State::Disconnected) return true;

    // Create command socket
    m_cmdSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_cmdSocket < 0) {
        std::cerr << "Failed to create command socket" << std::endl;
        return false;
    }

    // Set non-blocking for command socket
    int flags = fcntl(m_cmdSocket, F_GETFL, 0);
    fcntl(m_cmdSocket, F_SETFL, flags | O_NONBLOCK);

    // Bind command socket to allow receiving responses
    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(CMD_PORT);
    bind(m_cmdSocket, (struct sockaddr*)&localAddr, sizeof(localAddr));

    std::cout << "Connecting to Tello..." << std::endl;
    // Enter SDK mode
    if (!sendCommand("command", true)) {
        std::cerr << "Failed to enter SDK mode" << std::endl;
        ::close(m_cmdSocket);
        m_cmdSocket = -1;
        return false;
    }

    m_state = State::Connected;
    m_running = true;

    // Start state thread
    m_stateThread = std::thread(&TelloSDK::stateThreadFunc, this);
    // Start keepalive thread
    m_keepAliveThread = std::thread(&TelloSDK::keepAliveThreadFunc, this);

    std::cout << "Connected to Tello successfully." << std::endl;
    return true;
}

void TelloSDK::disconnect() {
    m_running = false;
    
    if (m_stateThread.joinable()) m_stateThread.join();
    if (m_videoThread.joinable()) m_videoThread.join();
    if (m_keepAliveThread.joinable()) m_keepAliveThread.join();

    if (m_cmdSocket >= 0) ::close(m_cmdSocket);
    if (m_stateSocket >= 0) ::close(m_stateSocket);
    if (m_videoSocket >= 0) ::close(m_videoSocket);

    m_cmdSocket = -1;
    m_stateSocket = -1;
    m_videoSocket = -1;
    m_state = State::Disconnected;
}

bool TelloSDK::sendCommand(const std::string& cmd, bool expectOk) {
    if (m_cmdSocket < 0) return false;

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(CMD_PORT);
    inet_pton(AF_INET, TELLO_IP.c_str(), &serverAddr.sin_addr);

    ssize_t sent = sendto(m_cmdSocket, cmd.c_str(), cmd.length(), 0, 
                          (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    
    if (sent < 0) return false;
    
    if (onResponse) onResponse("-> " + cmd);

    if (cmd.rfind("rc", 0) == 0) return true; // rc command returns no response

    std::string response = receiveResponse();
    if (onResponse) onResponse("<- " + response);

    if (expectOk) {
        return response.find("ok") != std::string::npos;
    }
    return !response.empty();
}

std::string TelloSDK::receiveResponse(int timeout_ms) {
    struct pollfd fd;
    fd.fd = m_cmdSocket;
    fd.events = POLLIN;

    int ret = poll(&fd, 1, timeout_ms);
    if (ret > 0) {
        char buffer[1024];
        sockaddr_in fromAddr;
        socklen_t fromLen = sizeof(fromAddr);
        ssize_t received = recvfrom(m_cmdSocket, buffer, sizeof(buffer) - 1, 0,
                                    (struct sockaddr*)&fromAddr, &fromLen);
        if (received > 0) {
            buffer[received] = '\0';
            std::string resp(buffer);
            // strip \r\n
            if (!resp.empty() && resp.back() == '\n') resp.pop_back();
            if (!resp.empty() && resp.back() == '\r') resp.pop_back();
            return resp;
        }
    }
    return "";
}

bool TelloSDK::takeoff() {
    if (sendCommand("takeoff")) {
        m_state = State::Flying;
        return true;
    }
    return false;
}

bool TelloSDK::land() {
    if (sendCommand("land")) {
        m_state = State::Connected;
        return true;
    }
    return false;
}

bool TelloSDK::emergency() {
    bool ok = sendCommand("emergency", false);
    m_state = State::Connected;
    return ok;
}

bool TelloSDK::moveForward(int cm) { return sendCommand("forward " + std::to_string(cm)); }
bool TelloSDK::moveBack(int cm) { return sendCommand("back " + std::to_string(cm)); }
bool TelloSDK::moveLeft(int cm) { return sendCommand("left " + std::to_string(cm)); }
bool TelloSDK::moveRight(int cm) { return sendCommand("right " + std::to_string(cm)); }
bool TelloSDK::moveUp(int cm) { return sendCommand("up " + std::to_string(cm)); }
bool TelloSDK::moveDown(int cm) { return sendCommand("down " + std::to_string(cm)); }
bool TelloSDK::rotateCW(int deg) { return sendCommand("cw " + std::to_string(deg)); }
bool TelloSDK::rotateCCW(int deg) { return sendCommand("ccw " + std::to_string(deg)); }
bool TelloSDK::flip(char direction) { return sendCommand(std::string("flip ") + direction); }
bool TelloSDK::setSpeed(int cms) { return sendCommand("speed " + std::to_string(cms)); }

bool TelloSDK::sendRC(int roll, int pitch, int throttle, int yaw) {
    std::string cmd = "rc " + std::to_string(roll) + " " + std::to_string(pitch) + 
                      " " + std::to_string(throttle) + " " + std::to_string(yaw);
    return sendCommand(cmd, false);
}

bool TelloSDK::streamOn() {
    if (sendCommand("streamon")) {
        m_videoSocket = socket(AF_INET, SOCK_DGRAM, 0);
        
        sockaddr_in localAddr{};
        localAddr.sin_family = AF_INET;
        localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        localAddr.sin_port = htons(VIDEO_PORT);
        
        bind(m_videoSocket, (struct sockaddr*)&localAddr, sizeof(localAddr));
        
        m_videoThread = std::thread(&TelloSDK::videoThreadFunc, this);
        if (m_state == State::Connected) m_state = State::Streaming;
        return true;
    }
    return false;
}

bool TelloSDK::streamOff() {
    if (sendCommand("streamoff")) {
        if (m_videoSocket >= 0) {
            ::close(m_videoSocket);
            m_videoSocket = -1;
        }
        if (m_videoThread.joinable()) m_videoThread.join();
        if (m_state == State::Streaming) m_state = State::Connected;
        return true;
    }
    return false;
}

int TelloSDK::queryBattery() {
    sendCommand("battery?", false);
    std::string resp = receiveResponse();
    try {
        return std::stoi(resp);
    } catch (...) {
        return -1;
    }
}

TelemetryData TelloSDK::getLatestTelemetry() {
    std::lock_guard<std::mutex> lock(m_telemetryMutex);
    return m_latestTelemetry;
}

TelemetryData TelloSDK::parseTelemetry(const std::string& data) {
    TelemetryData t;
    t.timestamp_ms = now_ms();
    
    std::stringstream ss(data);
    std::string token;
    
    while (std::getline(ss, token, ';')) {
        size_t colonPos = token.find(':');
        if (colonPos == std::string::npos) continue;
        
        std::string key = token.substr(0, colonPos);
        std::string valStr = token.substr(colonPos + 1);
        
        try {
            if (key == "pitch") t.pitch = std::stoi(valStr);
            else if (key == "roll") t.roll = std::stoi(valStr);
            else if (key == "yaw") t.yaw = std::stoi(valStr);
            else if (key == "vgx") t.vgx = std::stoi(valStr);
            else if (key == "vgy") t.vgy = std::stoi(valStr);
            else if (key == "vgz") t.vgz = std::stoi(valStr);
            else if (key == "templ") t.templ = std::stoi(valStr);
            else if (key == "temph") t.temph = std::stoi(valStr);
            else if (key == "tof") t.tof = std::stoi(valStr);
            else if (key == "h") t.h = std::stoi(valStr);
            else if (key == "bat") t.bat = std::stoi(valStr);
            else if (key == "baro") t.baro = std::stof(valStr);
            else if (key == "time") t.flightTime = std::stoi(valStr);
            else if (key == "agx") t.agx = std::stof(valStr);
            else if (key == "agy") t.agy = std::stof(valStr);
            else if (key == "agz") t.agz = std::stof(valStr);
        } catch (...) {
            // gracefully ignore parse errors
        }
    }
    return t;
}

void TelloSDK::stateThreadFunc() {
    m_stateSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_stateSocket < 0) return;

    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(STATE_PORT);
    
    if (bind(m_stateSocket, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        return;
    }

    struct pollfd fd;
    fd.fd = m_stateSocket;
    fd.events = POLLIN;

    char buffer[2048];
    while (m_running) {
        int ret = poll(&fd, 1, 500);
        if (ret > 0) {
            ssize_t received = recvfrom(m_stateSocket, buffer, sizeof(buffer) - 1, 0, nullptr, nullptr);
            if (received > 0) {
                buffer[received] = '\0';
                TelemetryData t = parseTelemetry(buffer);
                {
                    std::lock_guard<std::mutex> lock(m_telemetryMutex);
                    m_latestTelemetry = t;
                }
                if (onTelemetry) onTelemetry(t);
            }
        }
    }
}

void TelloSDK::videoThreadFunc() {
    struct pollfd fd;
    fd.fd = m_videoSocket;
    fd.events = POLLIN;

    uint8_t buffer[65535]; // Max UDP packet size
    while (m_running && m_videoSocket >= 0) {
        int ret = poll(&fd, 1, 500);
        if (ret > 0) {
            ssize_t received = recvfrom(m_videoSocket, buffer, sizeof(buffer), 0, nullptr, nullptr);
            if (received > 0 && onVideoData) {
                onVideoData(buffer, received);
            }
        }
    }
}

void TelloSDK::keepAliveThreadFunc() {
    // Tello requires a command every 15s to keep the connection alive
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        if (m_running && m_cmdSocket >= 0) {
            sendCommand("command", false);
        }
    }
}
