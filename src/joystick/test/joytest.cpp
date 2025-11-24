#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <errno.h>

// Uses evdev to poll for joystick events and print them out 

// Following is a axis:channel map for PocketMaster Radio to drive RealFlight sim:
// Throttle: Code 2
// Alieron/Roll: Code 0
// Rudder/Yaw: Code 3
// Elevator/Pitch:Code 1

int main() {
    const char* dev = "/dev/input/event0";

    int fd = open(dev, O_RDONLY);
    if (fd < 0) {
        std::cerr << "Failed to open " << dev 
                  << ": " << strerror(errno) << "\n";
        return 1;
    }

    std::cout << "Reading from " << dev << "...\n";
    std::cout << "Move sticks / press buttons.\n";

    input_event ev;

    while (true) {
        ssize_t n = read(fd, &ev, sizeof(ev));
        if (n == (ssize_t)-1) {
            if (errno == EINTR) continue;
            std::cerr << "Read error: " << strerror(errno) << "\n";
            break;
        }

        if (n != sizeof(ev)) {
            std::cerr << "Short read\n";
            break;
        }

        if (ev.type == EV_ABS) {
            std::cout << "[ABS] Code=" << ev.code 
                      << " Value=" << ev.value << "\n";
        }
        else if (ev.type == EV_KEY) {
            std::cout << "[KEY] Code=" << ev.code 
                      << " Value=" << ev.value << "\n";
        }
        else if (ev.type == EV_SYN) {
            // Sync frame, ignore
        }
    }

    close(fd);
    return 0;
}
