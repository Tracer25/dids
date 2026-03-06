CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread

TARGET = server
SRCS   = main.cpp aggregator.cpp listener.cpp kafka_producer.cpp metrics.cpp ../alert.cpp

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)
