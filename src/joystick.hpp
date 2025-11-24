#ifndef JOYSTICK_INTERFACE_HPP_
#define JOYSTICK_INTERFACE_HPP_

#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <cerrno>
#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

using namespace std::chrono;

namespace RF {

// Forward declarations for structs
struct RFCmd {
    double throttle;
    double aileron;
    double elevator; 
    double rudder;
    double flaps;
    double gear;
};


class Joystick {
public:
    explicit Joystick(const char* device = "/dev/input/event0") 
    : m_dev_path(device)
    {
        if (openDevice()) {
            std::cout << "[SUCCESS] Joystick: Successfully opened device: " << m_dev_path << std::endl;
            if(start_reading()) {
                std::cout << "[SUCCESS] Joystick: Started Reading from" << m_dev_path << std::endl;
            }
        }
    }

    ~Joystick() {
        if (m_joystick_read_thread.joinable()) {
            m_joystick_read_thread.join();
        }
        closeDevice();
    }

    bool start_reading() {
        if(m_fd > 0) {
            m_reading.store(true);
            m_joystick_read_thread = std::thread(&Joystick::pollForInputs, this);
            return true;
        }
        return false;
    }

    void stop_reading() {
        if(m_reading) m_reading.store(false);
    }

    bool is_reading() {
        return m_reading.load();
    }
  
    // Called from other threads
    RF::RFCmd getJoystickVals() {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        RFCmd new_state = m_state;
        return new_state;
    }

private:
    const char* CLASS = "JOYSTICK";
    const char* m_dev_path; 
    int m_fd = -1;

    std::atomic_bool m_reading{false};

    RFCmd m_state;
    std::thread m_joystick_read_thread;
    std::mutex m_state_mutex;


    static constexpr float AXIS_MIN = 0.0f;
    static constexpr float AXIS_MAX = 2040.0f;
    static constexpr float AXIS_CENTER = (AXIS_MIN + AXIS_MAX) /2.0f;
    static constexpr float AXIS_RANGE = (AXIS_MAX - AXIS_CENTER);

    bool openDevice() {
        m_fd = open(m_dev_path, O_RDONLY | O_NONBLOCK);
        if(m_fd < 0) {
            std::cerr << "[ERROR] Joystick: Failed to open "  << m_dev_path <<
                strerror(errno) << " \n";
            return false;
        }
        return true;
    }


    bool closeDevice() {
        if(m_fd >= 0) close(m_fd);
        m_fd = -1;
    }

    // Map [0, 2047] to a toggle 
    bool readJoystickDisable(float val) {
        return (val == 0);
    }


    // Map [0, 2040] to [-1, 1]
    float normalize(float val) {
        return (val - AXIS_CENTER) / AXIS_RANGE;
    }

    // Throttle wants [0, 1]
    float normalizeThrottle(float val) {
        if (val < AXIS_MIN) val = AXIS_MIN;
        if (val > AXIS_MAX) val = AXIS_MAX;
        return (val - AXIS_MIN) / (AXIS_MAX - AXIS_MIN);
    }


    // Code# to control mapping:
    //  Throttle: Code 2
    //  Alieron/Roll: Code 0
    //  Rudder/Yaw: Code 3
    //  Elevator/Pitch:Code 1
    //  Code 5: 3-way switch on top left
    void readAbs(int code, int value) {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        switch (code) {
            case 0:
                m_state.aileron = normalize(value);
                break;
            case 1:
                m_state.elevator = normalize(value);
                break;
            case 2:
                m_state.throttle = normalizeThrottle(value);
                break;
            case 3:
                m_state.rudder = normalize(value);
                break;
            case 5:
                // TODO: pass this disable to RFInterface
                readJoystickDisable(value);
                break;
        }
    }


void pollForInputs() {
    std::cout << "[INFO] Joystick polling thread started" << std::endl;
    
    while(m_reading.load()) {
        if (m_fd < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        struct input_event ev;
        ssize_t n = read(m_fd, &ev, sizeof(ev));
        
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data ready, wait a bit
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            } else {
                std::cerr << "[ERROR] Joystick read failed: " << strerror(errno) << std::endl;
                break;
            }
        }
        
        if (n == sizeof(ev)) {
            if (ev.type == EV_ABS) {
                readAbs(ev.code, ev.value);
            }
        }
    }
    
    std::cout << "[INFO] Joystick polling thread exiting" << std::endl;
}

};

}
#endif // JOYSTICK_INTERFACE_HPP_