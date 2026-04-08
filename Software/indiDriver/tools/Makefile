CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -I..

BUILD_DIR := build
BIN_DIR := $(BUILD_DIR)

BME_TARGET := $(BIN_DIR)/test_bme280
COM_TARGET := $(BIN_DIR)/test_astra_com
ONEWIRE_TARGET := $(BIN_DIR)/test_astra_1wire
STEPPWM_TARGET := $(BIN_DIR)/test_astra_step_pwm

BME_SRC := test_bme280.cpp ../astralim_bme280.cpp
COM_SRC := test_astra_com.cpp ../astralim_com_actor.cpp ../astralim_com_device.cpp ../astralim_com_fetcher.cpp
ONEWIRE_SRC := test_astra_1wire.cpp ../astralim_1wire_temp_fetcher.cpp
STEPPWM_SRC := test_astra_step_pwm.cpp ../astralim_step_pwm_actor.cpp ../astralim_com_actor.cpp ../astralim_com_device.cpp ../astralim_com_fetcher.cpp ../astralim_gpio.cpp

.PHONY: all bme com onewire steppwm run run-bme run-com run-1wire run-step clean

all: bme com onewire steppwm

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

bme: $(BME_TARGET)

com: $(COM_TARGET)

onewire: $(ONEWIRE_TARGET)

steppwm: $(STEPPWM_TARGET)

$(BME_TARGET): $(BME_SRC) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(BME_SRC) -o $@

$(COM_TARGET): $(COM_SRC) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(COM_SRC) -o $@ -pthread

$(ONEWIRE_TARGET): $(ONEWIRE_SRC) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(ONEWIRE_SRC) -o $@ -pthread

$(STEPPWM_TARGET): $(STEPPWM_SRC) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(STEPPWM_SRC) -o $@ -pthread -lgpiod

# Compatibilite historique: `make run` lance le test BME.
# Usage BME: make run-bme ARGS="0x76 1 5"
# Usage COM: make run-com ARGS="1200"
run: run-bme

run-bme: bme
	./$(BME_TARGET) $(ARGS)

run-com: com
	./$(COM_TARGET) $(ARGS)

run-1wire: onewire
	./$(ONEWIRE_TARGET) $(ARGS)

run-step: steppwm
	./$(STEPPWM_TARGET) $(ARGS)

clean:
	rm -rf $(BUILD_DIR)
