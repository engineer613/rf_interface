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

#include <queue>
#include <thread>
#include <mutex>
#include <chrono>
#include <iostream>
#include <sstream>

// Connection pool for managing sockets- Realflight does not allow using the same socket 
// for multiple SOAP requests according to docs floating around online
class SocketPool {
public:
    SocketPool(const char* ip, uint16_t port, size_t pool_size = 5) 
        : server_ip(ip), server_port(port), max_pool_size(pool_size), shutdown_flag(false) {
        // Start background thread to create connections
        pool_thread = std::thread(&SocketPool::maintain_pool, this);
        
        // Wait for initial connections
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    ~SocketPool() {
        shutdown_flag = true;
        if (pool_thread.joinable()) {
            pool_thread.join();
        }
        
        // Close all remaining sockets
        std::lock_guard<std::mutex> lock(pool_mutex);
        while (!available_sockets.empty()) {
            close(available_sockets.front());
            available_sockets.pop();
        }
    }
    
    int get_socket() {
        std::lock_guard<std::mutex> lock(pool_mutex);
        
        if (available_sockets.empty()) {
            // Create new connection if pool is empty
            return create_connection();
        }
        
        int sock = available_sockets.front();
        available_sockets.pop();
        return sock;
    }
  
private:
    int create_connection() {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
            return -1;
        }
        
        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(server_port);
        
        if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
            std::cerr << "Invalid address: " << server_ip << std::endl;
            close(sock);
            return -1;
        }
        
        // Set socket timeout
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        
        if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            std::cerr << "Connection failed: " << strerror(errno) << std::endl;
            close(sock);
            return -1;
        }
        
        return sock;
    }
    
    void maintain_pool() {
        while (!shutdown_flag) {
            std::unique_lock<std::mutex> lock(pool_mutex);
            size_t current_size = available_sockets.size();
            lock.unlock();
            
            if (current_size < max_pool_size) {
                int new_sock = create_connection();
                if (new_sock >= 0) {
                    lock.lock();
                    available_sockets.push(new_sock);
                    lock.unlock();
                }
            }
            
            // RF works better with no sleep
            // std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    
    const char* server_ip;
    uint16_t server_port;
    size_t max_pool_size;
    std::queue<int> available_sockets;
    std::mutex pool_mutex;
    std::thread pool_thread;
    bool shutdown_flag;
};