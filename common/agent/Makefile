CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread

TARGET = agent
SRCS   = main.cpp detector.cpp log_tail.cpp sender.cpp ../alert.cpp

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)
