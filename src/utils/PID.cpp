// src/utils/PID.cpp
#include "PID.h"

namespace {
    int64_t now_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
}

PID::PID(float p, float i, float d, float min, float max)
    : m_kp(p), m_ki(i), m_kd(d), m_outMin(min), m_outMax(max),
      m_integral(0), m_prevError(0), m_lastTime(0) {
}

float PID::update(float setpoint, float measured) {
    int64_t now = now_ms();
    if (m_lastTime == 0) {
        m_lastTime = now;
        return 0.0f;
    }

    float dt = (now - m_lastTime) / 1000.0f;
    if (dt <= 0.0f) dt = 0.001f;

    float error = setpoint - measured;
    
    m_integral += error * dt;
    
    // Anti-windup clamping (basic)
    // A better approach clamps based on whether the output is saturating
    if (m_ki * m_integral > m_outMax) m_integral = m_outMax / m_ki;
    else if (m_ki * m_integral < m_outMin) m_integral = m_outMin / m_ki;

    float derivative = (error - m_prevError) / dt;

    float output = (m_kp * error) + (m_ki * m_integral) + (m_kd * derivative);

    // Clamp output
    if (output > m_outMax) output = m_outMax;
    else if (output < m_outMin) output = m_outMin;

    m_prevError = error;
    m_lastTime = now;

    return output;
}

void PID::reset() {
    m_integral = 0;
    m_prevError = 0;
    m_lastTime = 0;
}
