#include "metrics.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

Metrics::Metrics(int port) : port_(port), server_fd_(-1), running_(false) {}

Metrics::~Metrics() {
    stop();
}

void Metrics::start() {
    if (running_) return;
    running_ = true;
    server_thread_ = std::thread(&Metrics::run_http_loop, this);
}

void Metrics::stop() {
    running_ = false;
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void Metrics::inc_connections() {
    ++connections_total_;
}

void Metrics::inc_disconnections() {
    ++disconnections_total_;
}

void Metrics::inc_alerts_received() {
    ++alerts_received_total_;
}

void Metrics::inc_alerts_malformed() {
    ++alerts_malformed_total_;
}

void Metrics::inc_alerts_published() {
    ++alerts_published_total_;
}

void Metrics::inc_alerts_publish_failed() {
    ++alerts_publish_failed_total_;
}

std::string Metrics::render_prometheus() const {
    std::string out;
    out += "# HELP dids_server_connections_total Total accepted agent connections\n";
    out += "# TYPE dids_server_connections_total counter\n";
    out += "dids_server_connections_total " + std::to_string(connections_total_.load()) + "\n";

    out += "# HELP dids_server_disconnections_total Total agent disconnections\n";
    out += "# TYPE dids_server_disconnections_total counter\n";
    out += "dids_server_disconnections_total " + std::to_string(disconnections_total_.load()) + "\n";

    out += "# HELP dids_server_alerts_received_total Total parsed alerts received\n";
    out += "# TYPE dids_server_alerts_received_total counter\n";
    out += "dids_server_alerts_received_total " + std::to_string(alerts_received_total_.load()) + "\n";

    out += "# HELP dids_server_alerts_malformed_total Total malformed alerts\n";
    out += "# TYPE dids_server_alerts_malformed_total counter\n";
    out += "dids_server_alerts_malformed_total " + std::to_string(alerts_malformed_total_.load()) + "\n";

    out += "# HELP dids_server_alerts_published_total Total alerts published to Kafka\n";
    out += "# TYPE dids_server_alerts_published_total counter\n";
    out += "dids_server_alerts_published_total " + std::to_string(alerts_published_total_.load()) + "\n";

    out += "# HELP dids_server_alerts_publish_failed_total Total Kafka publish failures\n";
    out += "# TYPE dids_server_alerts_publish_failed_total counter\n";
    out += "dids_server_alerts_publish_failed_total " + std::to_string(alerts_publish_failed_total_.load()) + "\n";

    return out;
}

void Metrics::run_http_loop() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        perror("[Metrics] socket");
        running_ = false;
        return;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[Metrics] bind");
        close(server_fd_);
        server_fd_ = -1;
        running_ = false;
        return;
    }

    if (listen(server_fd_, 16) < 0) {
        perror("[Metrics] listen");
        close(server_fd_);
        server_fd_ = -1;
        running_ = false;
        return;
    }

    std::cout << "[Metrics] Exposing Prometheus metrics on :" << port_ << std::endl;

    while (running_) {
        int client_fd = accept(server_fd_, nullptr, nullptr);
        if (client_fd < 0) {
            if (running_) perror("[Metrics] accept");
            continue;
        }

        char req_buf[2048];
        const ssize_t n = recv(client_fd, req_buf, sizeof(req_buf) - 1, 0);
        if (n <= 0) {
            close(client_fd);
            continue;
        }
        req_buf[n] = '\0';

        std::string request(req_buf);
        const bool is_metrics = request.rfind("GET /metrics", 0) == 0;
        std::string body = is_metrics ? render_prometheus() : "not found\n";

        std::string resp;
        if (is_metrics) {
            resp = "HTTP/1.1 200 OK\r\n";
            resp += "Content-Type: text/plain; version=0.0.4\r\n";
        } else {
            resp = "HTTP/1.1 404 Not Found\r\n";
            resp += "Content-Type: text/plain\r\n";
        }
        resp += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        resp += "Connection: close\r\n\r\n";
        resp += body;

        send(client_fd, resp.c_str(), resp.size(), 0);
        close(client_fd);
    }
}
