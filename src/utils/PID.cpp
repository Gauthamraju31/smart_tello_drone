#include "PID.h"
#include <algorithm>

PID::PID(float kp, float ki, float kd, float outMin, float outMax)
    : m_kp(kp), m_ki(ki), m_kd(kd), m_outMin(outMin), m_outMax(outMax),
      m_integral(0.0f), m_prevError(0.0f), m_lastTime(0) {}

float PID::update(float setpoint, float measured) {
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now().time_since_epoch())
                      .count();

    if (m_lastTime == 0) {
        m_lastTime = now_ms;
        return 0.0f; // Initial call, no dt yet
    }

    float dt = (now_ms - m_lastTime) / 1000.0f;
    if (dt <= 0.0f) {
        dt = 0.001f; // Prevent division by zero
    }
    
    m_lastTime = now_ms;

    float error = setpoint - measured;

    // Proportional
    float pOut = m_kp * error;

    // Integral
    m_integral += error * dt;
    // Anti-windup (clamp integral)
    float iOut = m_ki * m_integral;
    if (iOut > m_outMax) {
        iOut = m_outMax;
        m_integral = m_outMax / m_ki;
    } else if (iOut < m_outMin) {
        iOut = m_outMin;
        m_integral = m_outMin / m_ki;
    }

    // Derivative
    float derivative = (error - m_prevError) / dt;
    float dOut = m_kd * derivative;

    // Total output
    float output = pOut + iOut + dOut;

    // Clamp total output
    output = std::clamp(output, m_outMin, m_outMax);

    m_prevError = error;

    return output;
}

void PID::reset() {
    m_integral = 0.0f;
    m_prevError = 0.0f;
    m_lastTime = 0;
}
