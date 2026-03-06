#ifndef KAFKA_PRODUCER_H
#define KAFKA_PRODUCER_H

#include <cstdio>
#include <mutex>
#include <string>

// Simple Kafka producer that streams lines to kcat over stdin.
class KafkaProducer {
public:
    KafkaProducer(std::string brokers, std::string topic);
    ~KafkaProducer();

    bool connect();
    bool publish_line(const std::string& line);
    void disconnect();
    bool enabled() const;

private:
    std::string brokers_;
    std::string topic_;
    FILE* pipe_;
    std::mutex mutex_;
};

#endif // KAFKA_PRODUCER_H
