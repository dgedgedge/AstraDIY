/*******************************************************************************
 * AstrAlim GPIO Abstraction Layer Implementation
 * Supports libgpiod v1 and v2 APIs with automatic detection
 * Copyright (c) 2024 AstrAlim Project
 ******************************************************************************/

#include "astralim_gpio.h"
#include <gpiod.h>
#include <map>
#include <cstring>

namespace AstrAlim {

// Implementation structure - handles libgpiod version differences
struct GpioController::Impl {
#if GPIOD_VERSION_MAJOR >= 2
    // libgpiod v2 API
    struct gpiod_chip* chip = nullptr;
    std::map<int, struct gpiod_line_request*> requests;
    std::map<int, struct gpiod_line_config*> configs;
#else
    // libgpiod v1 API
    struct gpiod_chip* chip = nullptr;
    std::map<int, struct gpiod_line*> lines;
#endif
};

GpioController::GpioController() : pImpl(std::make_unique<Impl>()) {}

GpioController::~GpioController() {
    closeChip();
}

bool GpioController::openChip(const std::string& path) {
    if (pImpl->chip) {
        closeChip();
    }
    
#if GPIOD_VERSION_MAJOR >= 2
    pImpl->chip = gpiod_chip_open(path.c_str());
#else
    pImpl->chip = gpiod_chip_open(path.c_str());
#endif
    
    return pImpl->chip != nullptr;
}

void GpioController::closeChip() {
    if (!pImpl->chip) return;
    
    releaseAll();
    
#if GPIOD_VERSION_MAJOR >= 2
    gpiod_chip_close(pImpl->chip);
#else
    gpiod_chip_close(pImpl->chip);
#endif
    
    pImpl->chip = nullptr;
}

bool GpioController::isOpen() const {
    return pImpl->chip != nullptr;
}

bool GpioController::requestOutput(int pin, const std::string& consumer, int initial_value) {
    if (!pImpl->chip) return false;
    
#if GPIOD_VERSION_MAJOR >= 2
    // libgpiod v2 API
    struct gpiod_line_settings* settings = gpiod_line_settings_new();
    if (!settings) return false;
    
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, initial_value ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
    
    struct gpiod_line_config* config = gpiod_line_config_new();
    if (!config) {
        gpiod_line_settings_free(settings);
        return false;
    }
    
    unsigned int offsets[] = { static_cast<unsigned int>(pin) };
    gpiod_line_config_add_line_settings(config, offsets, 1, settings);
    
    struct gpiod_request_config* req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, consumer.c_str());
    
    struct gpiod_line_request* request = gpiod_chip_request_lines(pImpl->chip, req_cfg, config);
    
    gpiod_request_config_free(req_cfg);
    gpiod_line_settings_free(settings);
    
    if (!request) {
        gpiod_line_config_free(config);
        return false;
    }
    
    pImpl->requests[pin] = request;
    pImpl->configs[pin] = config;
    return true;
#else
    // libgpiod v1 API
    struct gpiod_line* line = gpiod_chip_get_line(pImpl->chip, pin);
    if (!line) return false;
    
    int ret = gpiod_line_request_output(line, consumer.c_str(), initial_value);
    if (ret < 0) return false;
    
    pImpl->lines[pin] = line;
    return true;
#endif
}

bool GpioController::requestInput(int pin, const std::string& consumer) {
    if (!pImpl->chip) return false;
    
#if GPIOD_VERSION_MAJOR >= 2
    struct gpiod_line_settings* settings = gpiod_line_settings_new();
    if (!settings) return false;
    
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    
    struct gpiod_line_config* config = gpiod_line_config_new();
    if (!config) {
        gpiod_line_settings_free(settings);
        return false;
    }
    
    unsigned int offsets[] = { static_cast<unsigned int>(pin) };
    gpiod_line_config_add_line_settings(config, offsets, 1, settings);
    
    struct gpiod_request_config* req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, consumer.c_str());
    
    struct gpiod_line_request* request = gpiod_chip_request_lines(pImpl->chip, req_cfg, config);
    
    gpiod_request_config_free(req_cfg);
    gpiod_line_settings_free(settings);
    
    if (!request) {
        gpiod_line_config_free(config);
        return false;
    }
    
    pImpl->requests[pin] = request;
    pImpl->configs[pin] = config;
    return true;
#else
    struct gpiod_line* line = gpiod_chip_get_line(pImpl->chip, pin);
    if (!line) return false;
    
    int ret = gpiod_line_request_input(line, consumer.c_str());
    if (ret < 0) return false;
    
    pImpl->lines[pin] = line;
    return true;
#endif
}

bool GpioController::releaseLine(int pin) {
#if GPIOD_VERSION_MAJOR >= 2
    auto req_it = pImpl->requests.find(pin);
    if (req_it != pImpl->requests.end()) {
        gpiod_line_request_release(req_it->second);
        pImpl->requests.erase(req_it);
    }
    
    auto cfg_it = pImpl->configs.find(pin);
    if (cfg_it != pImpl->configs.end()) {
        gpiod_line_config_free(cfg_it->second);
        pImpl->configs.erase(cfg_it);
    }
    return true;
#else
    auto it = pImpl->lines.find(pin);
    if (it != pImpl->lines.end()) {
        gpiod_line_release(it->second);
        pImpl->lines.erase(it);
        return true;
    }
    return false;
#endif
}

bool GpioController::setValue(int pin, int value) {
#if GPIOD_VERSION_MAJOR >= 2
    auto it = pImpl->requests.find(pin);
    if (it == pImpl->requests.end()) return false;
    
    enum gpiod_line_value val = value ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
    return gpiod_line_request_set_value(it->second, pin, val) == 0;
#else
    auto it = pImpl->lines.find(pin);
    if (it == pImpl->lines.end()) return false;
    
    return gpiod_line_set_value(it->second, value) == 0;
#endif
}

int GpioController::getValue(int pin) {
#if GPIOD_VERSION_MAJOR >= 2
    auto it = pImpl->requests.find(pin);
    if (it == pImpl->requests.end()) return -1;
    
    enum gpiod_line_value val = gpiod_line_request_get_value(it->second, pin);
    return (val == GPIOD_LINE_VALUE_ACTIVE) ? 1 : 0;
#else
    auto it = pImpl->lines.find(pin);
    if (it == pImpl->lines.end()) return -1;
    
    return gpiod_line_get_value(it->second);
#endif
}

bool GpioController::isLineUsed(int pin) {
    if (!pImpl->chip) return true;
    
#if GPIOD_VERSION_MAJOR >= 2
    struct gpiod_line_info* info = gpiod_chip_get_line_info(pImpl->chip, pin);
    if (!info) return true;
    
    bool used = gpiod_line_info_is_used(info);
    gpiod_line_info_free(info);
    return used;
#else
    struct gpiod_line* line = gpiod_chip_get_line(pImpl->chip, pin);
    if (!line) return true;
    
    return gpiod_line_is_used(line);
#endif
}

std::string GpioController::getLineConsumer(int pin) {
    if (!pImpl->chip) return "";
    
#if GPIOD_VERSION_MAJOR >= 2
    struct gpiod_line_info* info = gpiod_chip_get_line_info(pImpl->chip, pin);
    if (!info) return "";
    
    if (!gpiod_line_info_is_used(info)) {
        gpiod_line_info_free(info);
        return "";
    }
    
    const char* consumer = gpiod_line_info_get_consumer(info);
    std::string result = consumer ? consumer : "";
    gpiod_line_info_free(info);
    return result;
#else
    struct gpiod_line* line = gpiod_chip_get_line(pImpl->chip, pin);
    if (!line) return "";
    
    if (!gpiod_line_is_used(line)) {
        return "";
    }
    
    const char* consumer = gpiod_line_consumer(line);
    return consumer ? consumer : "";
#endif
}

bool GpioController::requestOutputBatch(const std::vector<GpioPin>& pins, const std::string& consumer) {
    for (const auto& pin : pins) {
        if (!requestOutput(pin.number, consumer, pin.initial_value)) {
            releaseAll();
            return false;
        }
    }
    return true;
}

void GpioController::releaseAll() {
#if GPIOD_VERSION_MAJOR >= 2
    for (auto& [pin, request] : pImpl->requests) {
        gpiod_line_request_release(request);
    }
    pImpl->requests.clear();
    
    for (auto& [pin, config] : pImpl->configs) {
        gpiod_line_config_free(config);
    }
    pImpl->configs.clear();
#else
    for (auto& [pin, line] : pImpl->lines) {
        gpiod_line_release(line);
    }
    pImpl->lines.clear();
#endif
}

} // namespace AstrAlim

