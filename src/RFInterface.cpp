#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>

#include <thread>
#include <chrono>
#include <iostream>
#include <sstream>

#include "RFInterface.hpp"
#include "joystick.hpp"

namespace RF {

// Static socket pool
static SocketPool* g_socket_pool = nullptr;

RFInterface::RFInterface(const char* rf_ip, uint16_t rf_port) 
    : rf_server_ip(rf_ip),
      rf_server_port(rf_port),
      sock_fd(-1),
      m_connected(false),
      m_joystick("/dev/input/event0") 
{
    memset(&state, 0, sizeof(state));
    memset(reply_buffer, 0, sizeof(reply_buffer));
    
    bool init_ok = false;

    // Initialize socket pool (pool size of 3 sockets)
    if (!g_socket_pool) {
        g_socket_pool = new SocketPool(rf_ip, rf_port, 3);
    }

    init_ok = (g_socket_pool == nullptr) ? false : true;
        
    if(init_ok &= connect()) {
        std::cout << "RFInterface initialized for " << rf_ip << ":" << rf_port << std::endl;
    }

    // while(!m_joystick.is_reading()) {
    //     // std::cout << "[UPDATE] RFInterface Waiting on Joystick to begin reading" << std::endl;
    //     // std::this_thread::sleep_for(std::chrono::milliseconds(10));
    // }

    if (init_ok) {
        m_update_thread = std::thread(&RFInterface::update, this);
        std::cout << "[SUCCESS] RFInterface Connected Successfully" <<  std::endl;
    } else {
        std::cout << "[ERROR] RFInterface Initialization Failed" <<  std::endl;
    }
}


RFInterface::~RFInterface() {
    disconnect();
    m_joystick.stop_reading();
}

bool RFInterface::isRFConnected() {
    return m_connected;
}


void RFInterface::update() {
    while(m_connected) {
        RFCmd cmd = m_joystick.getJoystickVals();
        // std::cout << "\n\n===========\n" << 
        // "Joy Command:\n" <<  
        // "Aileron: " << cmd.aileron << "\n" <<
        // "Elevator: " << cmd.elevator << "\n" <<
        // "Rudder: " << cmd.rudder << "\n" <<
        // "Throttle: " << cmd.throttle << "\n" <<
        // "=============\n\n" << std::endl;

        exchange_data(cmd);

        // Works better without sleep
        // std::this_thread::sleep_for(10ms);
    }
}


bool RFInterface::connect() {
    // Inject the UAV controller interface to take over from the internal RC
    const char* empty_body = "";
    if (!soap_request_start("InjectUAVControllerInterface", empty_body)) {
        std::cerr << "Failed to send InjectUAVControllerInterface request" << std::endl;
        return false;
    }
    
    char* response = soap_request_end(1000);
    if (!response) {
        std::cerr << "Failed to receive InjectUAVControllerInterface response" << std::endl;
        return false;
    }
    
    // Check if response indicates success (200 status)
    if (strstr(response, "200 OK") != nullptr) {
        std::cout << "External control enabled (RealFlight Link active)" << std::endl;
        m_connected = true;
        return true;
    }
    
    std::cerr << "InjectUAVControllerInterface request failed" << std::endl;
    return false;
}


bool RFInterface::disconnect() {
    // Restore the original controller device (joystick/RC)
    const char* empty_body = "";
    if (!soap_request_start("RestoreOriginalControllerDevice", empty_body)) {
        std::cerr << "Failed to send RestoreOriginalControllerDevice request" << std::endl;
        return false;
    }
    
    char* response = soap_request_end(1000);
    if (!response) {
        std::cerr << "Failed to receive RestoreOriginalControllerDevice response" << std::endl;
        return false;
    }
    
    // Check if response indicates success (200 status)
    if (strstr(response, "200 OK") != nullptr) {
        std::cout << "External control disabled (internal RC/joystick active)" << std::endl;
        m_connected = false;
        return true;
    }
    
    std::cerr << "RestoreOriginalControllerDevice request failed" << std::endl;
    return false;
}



bool RFInterface::reset_aircraft() {
    // Reset aircraft position (equivalent to pressing spacebar in RealFlight)
    const char* empty_body = "";
    if (!soap_request_start("ResetAircraft", empty_body)) {
        std::cerr << "Failed to send ResetAircraft request" << std::endl;
        return false;
    }
    
    char* response = soap_request_end(1000);
    if (!response) {
        std::cerr << "Failed to receive ResetAircraft response" << std::endl;
        return false;
    }
    
    // Check if response indicates success (200 status)
    if (strstr(response, "200 OK") != nullptr) {
        std::cout << "Aircraft reset to initial position" << std::endl;
        return true;
    }
    
    std::cerr << "ResetAircraft request failed" << std::endl;
    return false;
}


bool RFInterface::soap_request_start(const char *action, const char *fmt, ...) {
    // Get socket from pool
    sock_fd = g_socket_pool->get_socket();
    if (sock_fd < 0) {
        std::cerr << "Failed to get socket from pool" << std::endl;
        return false;
    }
    
    // Build the SOAP body
    char body[1024];
    if (fmt && strlen(fmt) > 0) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(body, sizeof(body), fmt, args);
        va_end(args);
    } else {
        body[0] = '\0';
    }
    
    // Build SOAP envelope
    std::stringstream envelope;
    envelope << "<?xml version='1.0' encoding='UTF-8'?>"
             << "<soap:Envelope xmlns:soap='http://schemas.xmlsoap.org/soap/envelope/' "
             << "xmlns:xsd='http://www.w3.org/2001/XMLSchema' "
             << "xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'>"
             << "<soap:Body>"
             << "<" << action << ">" << body << "</" << action << ">"
             << "</soap:Body>"
             << "</soap:Envelope>";
    
    std::string envelope_str = envelope.str();
    
    // Build HTTP request
    std::stringstream request;
    request << "POST / HTTP/1.1\r\n"
            << "Soapaction: '" << action << "'\r\n"
            << "Content-Length: " << envelope_str.length() << "\r\n"
            << "Content-Type: text/xml;charset=utf-8\r\n"
            << "\r\n"
            << envelope_str;
    
    std::string request_str = request.str();
    
    // Send request
    ssize_t sent = send(sock_fd, request_str.c_str(), request_str.length(), 0);
    if (sent < 0) {
        std::cerr << "Failed to send SOAP request: " << strerror(errno) << std::endl;
        close(sock_fd);
        sock_fd = -1;
        return false;
    }
    
    return true;
}


char* RFInterface::soap_request_end(uint32_t timeout_ms) {
    if (sock_fd < 0) {
        return nullptr;
    }
    
    memset(reply_buffer, 0, sizeof(reply_buffer));
    
    // Read response with timeout
    fd_set readfds;
    struct timeval tv;
    
    FD_ZERO(&readfds);
    FD_SET(sock_fd, &readfds);
    
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int ready = select(sock_fd + 1, &readfds, nullptr, nullptr, &tv);
    if (ready <= 0) {
        std::cerr << "Timeout or error waiting for response" << std::endl;
        close(sock_fd);
        sock_fd = -1;
        return nullptr;
    }
    
    // Read response
    size_t total_received = 0;
    ssize_t n;
    
    while (total_received < sizeof(reply_buffer) - 1) {
        n = recv(sock_fd, reply_buffer + total_received, sizeof(reply_buffer) - total_received - 1, 0);
        
        if (n <= 0) {
            break;
        }
        
        total_received += n;
        
        // Check if we've received the complete response
        if (strstr(reply_buffer, "</SOAP-ENV:Envelope>") != nullptr) {
            break;
        }
    }
    
    // Close socket (don't return to pool. RealFlight requires new connection per request)
    close(sock_fd);
    sock_fd = -1;
    
    if (total_received > 0) {
        reply_buffer[total_received] = '\0';
        return reply_buffer;
    }
    
    return nullptr;
}


void RFInterface::exchange_data(const struct RFCmd &input) {
    // Build control inputs XML
    std::stringstream body;
    body << "<pControlInputs>"
         << "<m-selectedChannels>4095</m-selectedChannels>"
         << "<m-channelValues-0to1>";
    
    // Map control inputs to channels (0.0 to 1.0 range)
    double channels[12] = {
        0.5,
        0.5,
        0.5, 
        0.5, 
        0.5, 
        0.5, 
        0.5, 
        0, // Mode: Stability Control toggle. Different modes, order for different planes
        0.5, 
        0.5, 
        0.5, 
        0.5};
    
    // Map inputs to appropriate channels
    channels[0] = input.aileron;   // Roll
    channels[1] = input.elevator;  // Pitch
    channels[2] = input.throttle;  // Throttle
    channels[3] = input.rudder;    // Yaw
    channels[4] = input.flaps;
    channels[5] = input.gear;
    
    for (int i = 0; i < 12; i++) {
        body << "<item>" << channels[i] << "</item>";
    }
    
    body << "</m-channelValues-0to1>"
         << "</pControlInputs>";
    
    std::string body_str = body.str();
    
    // Send SOAP request
    if (!soap_request_start("ExchangeData", body_str.c_str())) {
        std::cerr << "Failed to start SOAP request" << std::endl;
        return;
    }
    
    // Get response
    char* response = soap_request_end(1000);  // 1 second timeout
    
    if (response) {
        // std::cout << "\n=== Received SOAP Response ===" << std::endl;
        // std::cout << response << std::endl;
        // std::cout << "==============================\n" << std::endl;
        
        parse_reply(response);
    } else {
        std::cerr << "Failed to receive response" << std::endl;
    }
}

void RFInterface::parse_reply(const char *reply) {
    // Lambda to extract values from xml tag
    auto extract_value = [](const char* xml, const char* tag) -> double {
        std::string search_tag = std::string("<") + tag + ">";
        std::string end_tag = std::string("</") + tag + ">";
        
        const char* start = strstr(xml, search_tag.c_str());
        if (!start) return 0.0;
        
        start += search_tag.length();
        const char* end = strstr(start, end_tag.c_str());
        if (!end) return 0.0;
        
        std::string value_str(start, end - start);
        
        // Handle boolean values
        if (value_str == "true") return 1.0;
        if (value_str == "false") return 0.0;
        
        try {
            return std::stod(value_str);
        } catch (...) {
            return 0.0;
        }
    };
    
    // Parse all state values using the keytable
    for (int i = 0; i < num_keys; i++) {
        double value = extract_value(reply, keytable[i].key);
        keytable[i].ref = value;
    }
    
    // Print some key values
    // std::cout << "Aircraft State:" << std::endl;
    // std::cout << "  Airspeed: " << state.m_airspeed_MPS << " m/s" << std::endl;
    // std::cout << "  Altitude ASL: " << state.m_altitudeASL_MTR << " m" << std::endl;
    // std::cout << "  Altitude AGL: " << state.m_altitudeAGL_MTR << " m" << std::endl;
    // std::cout << "  Roll: " << state.m_roll_DEG << " deg" << std::endl;
    // std::cout << "  Pitch: " << state.m_inclination_DEG << " deg" << std::endl;
    // std::cout << "  Yaw: " << state.m_azimuth_DEG << " deg" << std::endl;
    // std::cout << "  Touching Ground: " << (state.m_isTouchingGround > 0.5 ? "Yes" : "No") << std::endl;
    // std::cout << "  Engine Running: " << (state.m_anEngineIsRunning > 0.5 ? "Yes" : "No") << std::endl;
}

} // namespace RF
