#ifndef AGGREGATOR_H
#define AGGREGATOR_H

#include "../alert.h"
#include <map>
#include <mutex>
#include <string>

// ------------------------------------------------------------
// Aggregator
// Collects alerts from all connected agents and tracks counts
// broken down by rule, agent, and severity.
//
// Because multiple agent-handler threads call add_alert() at
// the same time, every method that touches the counters uses
// a std::mutex to prevent data races.
// ------------------------------------------------------------
class Aggregator {
public:
    // Record one alert.  Thread-safe — may be called from any thread.
    void add_alert(const Alert& alert);

    // Print a human-readable summary to stdout.  Thread-safe.
    void print_summary() const;

private:
    mutable std::mutex mutex_; // protects all fields below

    int total_alerts_ = 0;

    // Each map key is the category label; value is the count.
    std::map<std::string, int> by_rule_;
    std::map<std::string, int> by_agent_;
    std::map<int, int>         by_severity_;
};

#endif // AGGREGATOR_H
