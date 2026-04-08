CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -I..
LDFLAGS :=

TARGET := test_bme280
SRC := test_bme280.cpp ../astralim_bme280.cpp

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $@ $(LDFLAGS)

# Usage:
# make run ARGS="0x76 1 5"
run: $(TARGET)
	./$(TARGET) $(ARGS)

clean:
	rm -f $(TARGET)
