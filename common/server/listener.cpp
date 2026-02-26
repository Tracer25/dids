#include "listener.h"
#include "../alert.h"
#include <iostream>
#include <thread>
#include <string>
#include <cstring>
#include <unistd.h>       // close()
#include <arpa/inet.h>    // inet_ntop(), htons()
#include <sys/socket.h>   // socket(), bind(), listen(), accept(), recv()
#include <netinet/in.h>   // sockaddr_in, INADDR_ANY

Listener::Listener(int port, Aggregator& aggregator)
    : port_(port), server_fd_(-1), aggregator_(aggregator), running_(false) {}

Listener::~Listener() {
    stop();
}

// ------------------------------------------------------------
// start
// Sets up the TCP socket that agents will connect to.
// Steps: create socket → set SO_REUSEADDR → bind → listen
// ------------------------------------------------------------
bool Listener::start() {
    // AF_INET = IPv4, SOCK_STREAM = TCP
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        perror("[Server] socket");
        return false;
    }

    // SO_REUSEADDR lets us restart the server quickly without waiting for
    // the OS to release the port from a previous run (TIME_WAIT state).
    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; // accept connections on any network interface
    addr.sin_port        = htons(port_);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[Server] bind");
        return false;
    }

    // listen() marks the socket as passive.  The second argument (10) is the
    // backlog — how many pending connections the OS will queue before refusing new ones.
    if (listen(server_fd_, 10) < 0) {
        perror("[Server] listen");
        return false;
    }

    running_ = true;
    std::cout << "[Server] Listening on port " << port_ << std::endl;
    return true;
}

// ------------------------------------------------------------
// run
// The accept-loop.  Runs on a background thread (launched in main).
// For every incoming agent, it spawns a detached thread that calls
// handle_client() — so this loop is free to accept the next agent
// immediately without waiting for the current one to finish.
// ------------------------------------------------------------
void Listener::run() {
    while (running_) {
        struct sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);

        // accept() blocks here until an agent connects, then returns a new
        // socket file descriptor that represents that specific connection.
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (running_) perror("[Server] accept");
            continue;
        }

        // Convert the binary IP address to a human-readable string for logging
        char ip_buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_buf, sizeof(ip_buf));
        std::string client_ip(ip_buf);

        std::cout << "[Server] Agent connected from " << client_ip << std::endl;

        // detach() means we don't need to call join() later — the thread
        // cleans itself up when handle_client() returns.
        std::thread(&Listener::handle_client, this, client_fd, client_ip).detach();
    }
}

// ------------------------------------------------------------
// stop
// Sets the running flag to false and closes the server socket,
// which causes the blocked accept() in run() to return with an error,
// letting the loop exit cleanly.
// ------------------------------------------------------------
void Listener::stop() {
    running_ = false;
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
}

// ------------------------------------------------------------
// handle_client
// Runs on its own thread — one per connected agent.
//
// TCP is a stream protocol: recv() may return partial lines or
// multiple lines at once.  We solve this with a buffer:
//   1. Append whatever recv() gives us to `buffer`.
//   2. Scan `buffer` for complete '\n'-terminated lines.
//   3. Process each complete line, leave any partial line in `buffer`.
// ------------------------------------------------------------
void Listener::handle_client(int client_fd, std::string client_ip) {
    std::string buffer; // accumulates raw bytes until we have a full line
    char chunk[4096];

    while (true) {
        ssize_t bytes = recv(client_fd, chunk, sizeof(chunk) - 1, 0);

        if (bytes <= 0) {
            // bytes == 0 → agent closed the connection cleanly
            // bytes <  0 → network error
            std::cout << "[Server] Agent " << client_ip << " disconnected." << std::endl;
            break;
        }

        chunk[bytes] = '\0';
        buffer += chunk; // append new data to our running buffer

        // Extract every complete line (terminated by '\n') from the buffer
        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, pos); // the complete JSON line
            buffer.erase(0, pos + 1);                 // remove it (+ the '\n') from buffer

            if (line.empty()) continue;

            // Parse the JSON line back into an Alert struct
            Alert alert = Alert::from_json(line);

            if (!alert.is_valid()) {
                std::cerr << "[Server] Malformed alert from " << client_ip
                          << ": " << line << std::endl;
                continue;
            }

            // Print a one-liner so we can see alerts arrive in real time
            std::cout << "[Alert] agent=" << alert.agent_id
                      << "  rule="        << alert.rule_name
                      << "  severity="    << alert.severity
                      << "  time="        << alert.timestamp
                      << std::endl;

            aggregator_.add_alert(alert);
        }
    }

    close(client_fd);
}
