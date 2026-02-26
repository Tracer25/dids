#ifndef ALERT_H
#define ALERT_H

#include <string>
#include <ctime>

struct Alert {
    std::string agent_id;
    std::string timestamp;
    std::string rule_name;
    int severity;
    std::string source_file;
    std::string original_log_line;

    // Serialize this alert to a JSON string for sending over the network
    std::string to_json() const;

    // Deserialize a JSON string (received from an agent) back into an Alert
    static Alert from_json(const std::string& json);

    // Returns true if the alert has the minimum required fields filled in
    bool is_valid() const;

private:
    // Escapes special characters so strings are safe to embed in JSON
    static std::string json_escape(const std::string& s);

    // Helpers used by from_json() to pull values out of a flat JSON string
    static std::string extract_string(const std::string& json, const std::string& key);
    static int         extract_int   (const std::string& json, const std::string& key);
};

#endif // ALERT_H
