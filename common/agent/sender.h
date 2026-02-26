#ifndef SENDER_H
#define SENDER_H

#include <string>

class Sender {
public:
    Sender(const std::string& server_ip, int port);
    ~Sender();
    bool connect_to_server();
    bool send_alert(const std::string& alert_json);
    void disconnect();

private:
    std::string server_ip_;
    int port_;
    int sock_;
};

#endif // SENDER_H