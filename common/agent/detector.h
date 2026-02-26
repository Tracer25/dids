#ifndef DETECTOR_H
#define DETECTOR_H

#include "../alert.h"
#include <string>
#include <vector>

class Detector {
public:
    Detector(const std::string& agent_id, const std::string& source_file);
    std::vector<Alert> detect(const std::string& log_line);

private:
    std::string agent_id_;
    std::string source_file_;
    std::string get_current_timestamp() const;
};

#endif // DETECTOR_H