// src/utils/PID.h
#pragma once

#include <chrono>

class PID {
public:
    PID(float kp, float ki, float kd, float outMin, float outMax);
    
    float update(float setpoint, float measured);
    void reset();

    void setGains(float p, float i, float d) { m_kp = p; m_ki = i; m_kd = d; }

private:
    float m_kp, m_ki, m_kd;
    float m_outMin, m_outMax;
    
    float m_integral;
    float m_prevError;
    int64_t m_lastTime;
};
