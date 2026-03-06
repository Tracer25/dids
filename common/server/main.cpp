#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include "listener.h"
#include "aggregator.h"
#include "kafka_producer.h"
#include "metrics.h"

// ------------------------------------------------------------
// Usage:
//   ./server [port] [summary_interval_seconds] [metrics_port]
//
// Defaults:
//   port     = 9000
//   interval = 10 seconds
//   metrics  = 9464
// ------------------------------------------------------------
int main(int argc, char* argv[]) {
    int port             = (argc > 1) ? std::stoi(argv[1]) : 9000;
    int summary_interval = (argc > 2) ? std::stoi(argv[2]) : 10;
    int metrics_port     = (argc > 3) ? std::stoi(argv[3]) : 9464;

    const char* kafka_brokers_env = std::getenv("KAFKA_BROKERS");
    const char* kafka_topic_env = std::getenv("KAFKA_TOPIC");
    std::string kafka_brokers = kafka_brokers_env ? kafka_brokers_env : "";
    std::string kafka_topic = kafka_topic_env ? kafka_topic_env : "alerts.raw";

    // The aggregator is shared between the listener threads.
    // It uses a mutex internally so concurrent writes are safe.
    Aggregator aggregator;

    Metrics metrics(metrics_port);
    metrics.start();

    KafkaProducer kafka(kafka_brokers, kafka_topic);
    if (kafka.enabled() && !kafka.connect()) {
        std::cerr << "[Server] Kafka enabled but producer failed to start." << std::endl;
    }

    // The listener owns the TCP socket and spawns one thread per agent.
    Listener listener(port, aggregator, &kafka, &metrics);

    if (!listener.start()) {
        std::cerr << "[Server] Failed to start. Exiting." << std::endl;
        return 1;
    }

    // Run the accept-loop on a background thread so the main thread
    // is free to print the periodic summary below.
    std::thread listener_thread([&listener]() {
        listener.run();
    });
    listener_thread.detach(); // we never need to join it

    std::cout << "[Server] Printing summary every " << summary_interval << " seconds.\n";

    // Main thread just sleeps and prints summaries forever.
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(summary_interval));
        aggregator.print_summary();
    }

    return 0;
}
