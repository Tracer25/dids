#ifndef LISTENER_H
#define LISTENER_H

#include "aggregator.h"
#include "kafka_producer.h"
#include "metrics.h"
#include <string>
#include <atomic>

// ------------------------------------------------------------
// Listener
// Owns the server TCP socket.  Accepts incoming agent connections
// and spawns a new thread for each one so multiple agents can
// send alerts at the same time without blocking each other.
// ------------------------------------------------------------
class Listener {
public:
    // Pass the port to bind on and a reference to the shared aggregator.
    Listener(int port, Aggregator& aggregator, KafkaProducer* kafka, Metrics* metrics);
    ~Listener();

    // Creates the socket, binds it, and starts listening.
    // Returns true on success, false on any socket error.
    bool start();

    // Blocking accept-loop.  Run this on a background thread from main().
    // Keeps looping until stop() is called.
    void run();

    // Signals the accept-loop to exit and closes the server socket.
    void stop();

private:
    // Called on its own thread for each connected agent.
    // Reads newline-delimited JSON, parses each line into an Alert,
    // and passes valid alerts to the aggregator.
    void handle_client(int client_fd, std::string client_ip);

    int        port_;
    int        server_fd_;       // the server-side listening socket
    Aggregator& aggregator_;     // shared — access is guarded inside Aggregator
    KafkaProducer* kafka_;       // optional kafka publisher
    Metrics* metrics_;           // optional metrics collector
    std::atomic<bool> running_;  // thread-safe flag to stop the loop
};

#endif // LISTENER_H
