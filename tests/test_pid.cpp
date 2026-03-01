#include "../src/utils/PID.h"
#include <iostream>
#include <cassert>
#include <thread>

void test_pid_basic() {
    PID pid(1.0f, 0.0f, 0.0f, -100.0f, 100.0f);
    
    // First call returns 0 because dt is not established
    float out1 = pid.update(10.0f, 0.0f);
    assert(out1 == 0.0f);
    
    // Sleep to create a dt > 0
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Proportional only: Error is 10, Kp is 1.0 => output should be 10.0
    float out2 = pid.update(10.0f, 0.0f);
    assert(out2 > 9.9f && out2 < 10.1f);
    
    std::cout << "test_pid_basic passed.\n";
}

void test_pid_clamping() {
    PID pid(100.0f, 0.0f, 0.0f, -50.0f, 50.0f);
    pid.update(100.0f, 0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Error is 100, Kp is 100 => 10000. Clamped to 50.
    float out = pid.update(100.0f, 0.0f);
    assert(out == 50.0f);
    
    std::cout << "test_pid_clamping passed.\n";
}

int main() {
    std::cout << "Running PID tests...\n";
    test_pid_basic();
    test_pid_clamping();
    std::cout << "All PID tests passed!\n";
    return 0;
}
