#include "../alert.h"
#include "log_tail.h"
#include "detector.h"
#include "sender.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <agent_id> <log_file> [server_ip] [port]" << std::endl;
        return 1;
    }

    std::string agent_id = argv[1];
    std::string log_file = argv[2];
    std::string server_ip = (argc > 3) ? argv[3] : "127.0.0.1";
    int port = (argc > 4) ? std::stoi(argv[4]) : 9000;

    LogTail tail(log_file);
    Detector detector(agent_id, log_file);
    Sender sender(server_ip, port);

    if (!sender.connect_to_server()) {
        std::cerr << "Failed to connect to server. Retrying..." << std::endl;
    }

    while (true) {
        std::string line = tail.get_next_line();
        if (!line.empty()) {
            std::vector<Alert> alerts = detector.detect(line);
            for (const auto& alert : alerts) {
                std::string json = alert.to_json();
                if (!sender.send_alert(json)) {
                    std::cerr << "Failed to send alert. Retrying connection..." << std::endl;
                    sender.disconnect();
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    if (!sender.connect_to_server()) {
                        std::cerr << "Reconnection failed." << std::endl;
                    } else {
                        std::cout << "Reconnected to server." << std::endl;
                        // Retry sending the alert
                        if (!sender.send_alert(json)) {
                            std::cerr << "Failed to send alert after reconnection." << std::endl;
                        }
                    }
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Poll every 100ms
    }

    return 0;
}