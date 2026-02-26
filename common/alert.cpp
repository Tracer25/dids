#include "alert.h"
#include <sstream>
#include <cctype>

// ------------------------------------------------------------
// json_escape
// Replaces characters that would break a JSON string literal
// (quotes, backslashes, newlines, etc.) with their escape sequences.
// ------------------------------------------------------------
std::string Alert::json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// ------------------------------------------------------------
// to_json
// Builds the NDJSON line that gets sent over TCP.
// Example output (one line):
//   {"agent_id":"web-01","ts":"2026-02-22T15:00:00Z","rule":"SSH_FAILED_PASSWORD","severity":3,"src":"auth.log","line":"Failed password for root"}
// ------------------------------------------------------------
std::string Alert::to_json() const {
    std::ostringstream oss;
    oss << "{"
        << "\"agent_id\":\""  << json_escape(agent_id)           << "\","
        << "\"ts\":\""        << json_escape(timestamp)          << "\","
        << "\"rule\":\""      << json_escape(rule_name)          << "\","
        << "\"severity\":"    << severity                        << ","
        << "\"src\":\""       << json_escape(source_file)        << "\","
        << "\"line\":\""      << json_escape(original_log_line)  << "\""
        << "}";
    return oss.str();
}

// ------------------------------------------------------------
// extract_string
// Finds  "key":"value"  inside a JSON string and returns value.
// Handles escaped characters (e.g. \" inside the value).
// Returns "" if the key isn't found.
// ------------------------------------------------------------
std::string Alert::extract_string(const std::string& json, const std::string& key) {
    // Search for  "key":"
    std::string search = "\"" + key + "\":\"";
    size_t start = json.find(search);
    if (start == std::string::npos) return "";
    start += search.size(); // move past the opening quote of the value

    std::string result;
    for (size_t i = start; i < json.size(); ++i) {
        if (json[i] == '\\' && i + 1 < json.size()) {
            // Handle escape sequences produced by json_escape()
            char next = json[i + 1];
            if      (next == '"')  { result += '"';  ++i; }
            else if (next == '\\') { result += '\\'; ++i; }
            else if (next == 'n')  { result += '\n'; ++i; }
            else if (next == 'r')  { result += '\r'; ++i; }
            else if (next == 't')  { result += '\t'; ++i; }
            else                   { result += json[i]; }
        } else if (json[i] == '"') {
            // Closing quote — we're done
            break;
        } else {
            result += json[i];
        }
    }
    return result;
}

// ------------------------------------------------------------
// extract_int
// Finds  "key":NUMBER  inside a JSON string and returns NUMBER.
// Returns 0 if the key isn't found.
// ------------------------------------------------------------
int Alert::extract_int(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t start = json.find(search);
    if (start == std::string::npos) return 0;
    start += search.size();

    // Collect consecutive digit characters
    std::string num;
    for (size_t i = start; i < json.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(json[i]))) {
            num += json[i];
        } else if (!num.empty()) {
            break; // stop as soon as we leave the number
        }
    }
    return num.empty() ? 0 : std::stoi(num);
}

// ------------------------------------------------------------
// from_json
// Parses a single NDJSON line received from an agent back into
// an Alert struct.  Mirrors the field names written by to_json().
// ------------------------------------------------------------
Alert Alert::from_json(const std::string& json) {
    Alert a;
    a.agent_id          = extract_string(json, "agent_id");
    a.timestamp         = extract_string(json, "ts");
    a.rule_name         = extract_string(json, "rule");
    a.severity          = extract_int   (json, "severity");
    a.source_file       = extract_string(json, "src");
    a.original_log_line = extract_string(json, "line");
    return a;
}

// ------------------------------------------------------------
// is_valid
// A minimal sanity check before the server stores an alert.
// ------------------------------------------------------------
bool Alert::is_valid() const {
    return !agent_id.empty() && !rule_name.empty();
}
