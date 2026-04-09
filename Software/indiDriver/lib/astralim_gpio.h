/*******************************************************************************
 * AstrAlim GPIO Abstraction Layer
 * Supports both libgpiod v1 and v2 APIs
 * Copyright (c) 2024 AstrAlim Project
 ******************************************************************************/

#ifndef ASTRALIM_GPIO_H
#define ASTRALIM_GPIO_H

#include <string>
#include <memory>
#include <vector>

namespace AstrAlim {

// GPIO Pin configuration
struct GpioPin {
    int number;
    std::string name;
    bool is_output;
    int initial_value;
};

// GPIO abstraction class
class GpioController {
public:
    GpioController();
    ~GpioController();

    // Chip operations
    bool openChip(const std::string& path = "/dev/gpiochip4");
    void closeChip();
    bool isOpen() const;

    // Line operations
    bool requestOutput(int pin, const std::string& consumer, int initial_value = 0);
    bool requestInput(int pin, const std::string& consumer);
    bool releaseLine(int pin);
    
    // Value operations
    bool setValue(int pin, int value);
    int getValue(int pin);
    
    // Check if line is in use
    bool isLineUsed(int pin);
    
    // Get consumer name if line is in use (empty string if not used)
    std::string getLineConsumer(int pin);

    // Batch operations
    bool requestOutputBatch(const std::vector<GpioPin>& pins, const std::string& consumer);
    void releaseAll();

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

// RPi5 GPIO chip path
constexpr const char* RPI5_GPIO_CHIP = "/dev/gpiochip4";

// AstrAlim Focuser GPIO pins (BCM numbering)
namespace FocuserPins {
    constexpr int DIR   = 10;
    constexpr int STEP  = 24;
    constexpr int SLEEP = 23;
    constexpr int M1    = 11;
    constexpr int M2    = 7;
    constexpr int M3    = 5;
}

// AstrAlim Relay GPIO pins (BCM numbering)
namespace RelayPins {
    constexpr int DC1 = 26;
    constexpr int DC2 = 20;
    constexpr int DC3 = 21;
}

} // namespace AstrAlim

#endif // ASTRALIM_GPIO_H

