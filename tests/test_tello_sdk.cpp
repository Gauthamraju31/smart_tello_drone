#include "../src/core/TelloSDK.h"
#include <iostream>
#include <cassert>

// A light integration/structure test for TelloSDK since actual
// network sockets to 192.168.10.1 cannot be mocked easily 
// without dependency injection of the socket layer.

void test_sdk_initial_state() {
    TelloSDK sdk;
    
    // Ensure it starts disconnected
    assert(sdk.getState() == TelloSDK::State::Disconnected);
    
    // Ensure default telemetry is zeroed out
    TelemetryData t = sdk.getLatestTelemetry();
    assert(t.pitch == 0);
    assert(t.bat == 0);
    assert(t.baro == 0.0f);
    
    std::cout << "test_sdk_initial_state passed.\n";
}

int main() {
    std::cout << "Running TelloSDK tests...\n";
    test_sdk_initial_state();
    std::cout << "All TelloSDK tests passed!\n";
    return 0;
}
