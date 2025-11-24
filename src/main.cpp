// Program to test out the RFInterface class

#include "RFInterface.hpp"
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
    
    // Create RF interface with Joystick 
    RFInterface sim("172.19.112.1", 18083);
    
    while (running) {              
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
