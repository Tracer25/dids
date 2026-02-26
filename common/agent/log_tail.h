#ifndef LOG_TAIL_H
#define LOG_TAIL_H

#include <string>
#include <fstream>

class LogTail {
public:
    LogTail(const std::string& filename);
    ~LogTail();
    std::string get_next_line();

private:
    std::ifstream file_;
    std::streampos last_pos_;
};

#endif // LOG_TAIL_H