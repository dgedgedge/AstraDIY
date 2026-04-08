CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -I..

BUILD_DIR := build
BIN_DIR := $(BUILD_DIR)

BME_TARGET := $(BIN_DIR)/test_bme280
COM_TARGET := $(BIN_DIR)/test_astra_com

BME_SRC := test_bme280.cpp ../astralim_bme280.cpp
COM_SRC := test_astra_com.cpp ../astralim_com_actor.cpp ../astralim_com_device.cpp ../astralim_com_fetcher.cpp

.PHONY: all bme com run run-bme run-com clean

all: bme com

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

bme: $(BME_TARGET)

com: $(COM_TARGET)

$(BME_TARGET): $(BME_SRC) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(BME_SRC) -o $@

$(COM_TARGET): $(COM_SRC) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(COM_SRC) -o $@ -pthread

# Compatibilite historique: `make run` lance le test BME.
# Usage BME: make run-bme ARGS="0x76 1 5"
# Usage COM: make run-com ARGS="1200"
run: run-bme

run-bme: bme
	./$(BME_TARGET) $(ARGS)

run-com: com
	./$(COM_TARGET) $(ARGS)

clean:
	rm -rf $(BUILD_DIR)
