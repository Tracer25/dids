#include "aggregator.h"
#include <iostream>

// ------------------------------------------------------------
// add_alert
// Called every time the listener receives and parses a valid alert.
// The lock_guard locks the mutex when it's constructed and
// automatically unlocks it when it goes out of scope — you never
// forget to unlock, even if an exception is thrown.
// ------------------------------------------------------------
void Aggregator::add_alert(const Alert& alert) {
    std::lock_guard<std::mutex> lock(mutex_);

    total_alerts_++;
    by_rule_[alert.rule_name]++;
    by_agent_[alert.agent_id]++;
    by_severity_[alert.severity]++;
}

// ------------------------------------------------------------
// print_summary
// Dumps the current counters.  Declared `const` because it
// doesn't modify data — it only reads it.  The mutex is still
// needed so we don't read partially-updated counters.
// ------------------------------------------------------------
void Aggregator::print_summary() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::cout << "\n--- Alert Summary ---\n";
    std::cout << "Total alerts: " << total_alerts_ << "\n";

    std::cout << "By rule:\n";
    for (const auto& [rule, count] : by_rule_) {
        std::cout << "  " << rule << ": " << count << "\n";
    }

    std::cout << "By agent:\n";
    for (const auto& [agent, count] : by_agent_) {
        std::cout << "  " << agent << ": " << count << "\n";
    }

    std::cout << "By severity:\n";
    for (const auto& [sev, count] : by_severity_) {
        std::cout << "  [" << sev << "]: " << count << "\n";
    }

    std::cout << "----------------------\n";
}
