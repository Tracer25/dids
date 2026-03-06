#include "log_tail.h"
#include <iostream>


//a file follower that watches the log file and returns only the new lines as they get 
//appended, skips everything else 

LogTail::LogTail(const std::string& filename) : last_pos_(0) {
    file_.open(filename);
    if (!file_.is_open()) {
        std::cerr << "Failed to open log file: " << filename << std::endl;
        return;
    }
    file_.seekg(0, std::ios::end);
    last_pos_ = file_.tellg();
}

LogTail::~LogTail() {
    if (file_.is_open()) {
        file_.close();
    }
}

std::string LogTail::get_next_line() {
    if (!file_.is_open()) {
        return "";
    }

    //change the input pointer to last position

    file_.seekg(last_pos_);
    std::string line;
    if (std::getline(file_, line)) {
        last_pos_ = file_.tellg();
        return line;
    }
    return "";
}