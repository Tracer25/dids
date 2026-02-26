#include "detector.h"
#include <ctime>
#include <sstream>
#include <iomanip>

Detector::Detector(const std::string& agent_id, const std::string& source_file)
    : agent_id_(agent_id), source_file_(source_file) {}

std::vector<Alert> Detector::detect(const std::string& log_line) {
    std::vector<Alert> alerts;

    // Rule 1: SSH_FAILED_PASSWORD
    if (log_line.find("Failed password") != std::string::npos) {
        Alert alert;
        alert.agent_id = agent_id_;
        alert.timestamp = get_current_timestamp();
        alert.rule_name = "SSH_FAILED_PASSWORD";
        alert.severity = 3;
        alert.source_file = source_file_;
        alert.original_log_line = log_line;
        alerts.push_back(alert);
    }

    // Rule 2: SUDO_USED
    if (log_line.find("sudo") != std::string::npos) {
        Alert alert;
        alert.agent_id = agent_id_;
        alert.timestamp = get_current_timestamp();
        alert.rule_name = "SUDO_USED";
        alert.severity = 2;
        alert.source_file = source_file_;
        alert.original_log_line = log_line;
        alerts.push_back(alert);
    }

    // Rule 3: WEB_SCAN
    if (log_line.find("../") != std::string::npos || log_line.find("wp-admin") != std::string::npos) {
        Alert alert;
        alert.agent_id = agent_id_;
        alert.timestamp = get_current_timestamp();
        alert.rule_name = "WEB_SCAN";
        alert.severity = 4;
        alert.source_file = source_file_;
        alert.original_log_line = log_line;
        alerts.push_back(alert);
    }

    return alerts;
}

std::string Detector::get_current_timestamp() const {
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::gmtime(&now);
    std::ostringstream oss;
    oss << (tm->tm_year + 1900) << "-"
        << std::setfill('0') << std::setw(2) << (tm->tm_mon + 1) << "-"
        << std::setfill('0') << std::setw(2) << tm->tm_mday << "T"
        << std::setfill('0') << std::setw(2) << tm->tm_hour << ":"
        << std::setfill('0') << std::setw(2) << tm->tm_min << ":"
        << std::setfill('0') << std::setw(2) << tm->tm_sec << "Z";
    return oss.str();
}