#include "kafka_producer.h"
#include <iostream>

KafkaProducer::KafkaProducer(std::string brokers, std::string topic)
    : brokers_(std::move(brokers)), topic_(std::move(topic)), pipe_(nullptr) {}

KafkaProducer::~KafkaProducer() {
    disconnect();
}

bool KafkaProducer::enabled() const {
    return !brokers_.empty() && !topic_.empty();
}

bool KafkaProducer::connect() {
    if (!enabled()) return false;

    std::lock_guard<std::mutex> lock(mutex_);
    if (pipe_) return true;

    // kcat reads lines from stdin and sends one message per line.
    const std::string cmd = "kcat -P -b " + brokers_ + " -t " + topic_ + " 2>/tmp/dids-kcat.err";
    pipe_ = popen(cmd.c_str(), "w");
    if (!pipe_) {
        std::cerr << "[Kafka] Failed to start kcat producer process." << std::endl;
        return false;
    }

    std::cout << "[Kafka] Producer connected to brokers=" << brokers_
              << " topic=" << topic_ << std::endl;
    return true;
}

bool KafkaProducer::publish_line(const std::string& line) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!pipe_) return false;
    if (fputs(line.c_str(), pipe_) == EOF) return false;
    if (fputc('\n', pipe_) == EOF) return false;
    return fflush(pipe_) == 0;
}

void KafkaProducer::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!pipe_) return;

    pclose(pipe_);
    pipe_ = nullptr;
}
