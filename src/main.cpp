// Program to test out the RFInterface class

#include "RFInterface.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>

using namespace RF;

volatile bool running = true;

void signal_handler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Shutting down...\n";
    running = false;
}

int main(int argc, char* argv[]) {
    // signal handler for clean exit
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "=== RealFlight C++ Interface Test ===" << std::endl;
    std::cout << "Attempting to connect to RealFlight simulator..." << std::endl;
    std::cout << "Press Ctrl+C to exit\n" << std::endl;
    
    // Default connection parameters
    const char* rf_ip = "127.0.0.1";
    uint16_t rf_port = 18083;  // RealFlight default SOAP port
    
    // Allow override from command line
    if (argc >= 2) {
        rf_ip = argv[1];
    }
    if (argc >= 3) {
        rf_port = static_cast<uint16_t>(std::stoi(argv[2]));
    }
    
    std::cout << "Connecting to: " << rf_ip << ":" << rf_port << std::endl;
    
    // Create RF interface
    RFInterface rf(rf_ip, rf_port);
    
    // Setup control inputs - start with neutral/idle
    control_input input;
    input.throttle = 0.0;   // Idle throttle
    input.aileron = 0.5;    // Neutral aileron
    input.elevator = 0.5;   // Neutral elevator
    input.rudder = 0.5;     // Neutral rudder
    input.flaps = 0.0;      // No flaps
    input.gear = 0.0;       // Gear down
    
    std::cout << "\nAttempting to connect and exchange data..." << std::endl;
    std::cout << "Initial control inputs:" << std::endl;
    std::cout << "  Throttle: " << input.throttle << std::endl;
    std::cout << "  Aileron:  " << input.aileron << std::endl;
    std::cout << "  Elevator: " << input.elevator << std::endl;
    std::cout << "  Rudder:   " << input.rudder << std::endl;
    std::cout << "\nWaiting for response from RealFlight...\n" << std::endl;
    
    int update_count = 0;
    
    while (running) {
        // Call update which will handle SOAP communication
        rf.update(input);
        
        update_count++;
        
        if (update_count == 1) {
            std::cout << "\n[SUCCESS] Connected and received first response!" << std::endl;
            std::cout << "Hit Ctrl+C to exit..\n" << std::endl;
        }
        
        // Gradually increase throttle as a test
        if (input.throttle < 1) {
            input.throttle += 0.03;
        }
        
        // Works better without sleep in this thread
        // std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // std::cout << "\n=== Test Complete ===" << std::endl;
    // std::cout << "Total updates: " << update_count << std::endl;
    // std::cout << "Final aircraft state:" << std::endl;
    // std::cout << "  Position X: " << rf.state.m_aircraftPositionX_MTR << " m" << std::endl;
    // std::cout << "  Position Y: " << rf.state.m_aircraftPositionY_MTR << " m" << std::endl;
    // std::cout << "  Altitude:   " << rf.state.m_altitudeAGL_MTR << " m" << std::endl;
    // std::cout << "  Airspeed:   " << rf.state.m_airspeed_MPS << " m/s" << std::endl;
    
    return 0;
}
