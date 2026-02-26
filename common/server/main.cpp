#include <iostream>
#include <thread>
#include <chrono>
#include "listener.h"
#include "aggregator.h"

// ------------------------------------------------------------
// Usage:
//   ./server [port] [summary_interval_seconds]
//
// Defaults:
//   port     = 9000
//   interval = 10 seconds
// ------------------------------------------------------------
int main(int argc, char* argv[]) {
    int port             = (argc > 1) ? std::stoi(argv[1]) : 9000;
    int summary_interval = (argc > 2) ? std::stoi(argv[2]) : 10;

    // The aggregator is shared between the listener threads.
    // It uses a mutex internally so concurrent writes are safe.
    Aggregator aggregator;

    // The listener owns the TCP socket and spawns one thread per agent.
    Listener listener(port, aggregator);

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
