#ifndef METRICS_H
#define METRICS_H

#include <atomic>
#include <string>
#include <thread>

// Small Prometheus text-format metrics endpoint for the server.
class Metrics {
public:
    explicit Metrics(int port);
    ~Metrics();

    void start();
    void stop();

    void inc_connections();
    void inc_disconnections();
    void inc_alerts_received();
    void inc_alerts_malformed();
    void inc_alerts_published();
    void inc_alerts_publish_failed();

private:
    void run_http_loop();
    std::string render_prometheus() const;

    int port_;
    int server_fd_;
    std::atomic<bool> running_;
    std::thread server_thread_;

    std::atomic<unsigned long long> connections_total_{0};
    std::atomic<unsigned long long> disconnections_total_{0};
    std::atomic<unsigned long long> alerts_received_total_{0};
    std::atomic<unsigned long long> alerts_malformed_total_{0};
    std::atomic<unsigned long long> alerts_published_total_{0};
    std::atomic<unsigned long long> alerts_publish_failed_total_{0};
};

#endif // METRICS_H
