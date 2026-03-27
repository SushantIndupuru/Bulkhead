#pragma once
#include <Arduino.h>

enum IndicatorState : uint8_t {
    INDICATOR_OFF,
    LEFT,
    RIGHT,
    HAZARDS
};

// packet type 1 – data from bulkhead to steeringwheel
struct __attribute__((packed)) ForwardPacket {
    uint8_t speed; //mph
    uint16_t voltage; //volts stored at int, converted from 12.34 to 1234 to save space
};

// packet type 2 – from steeringwheel to bulkhead
struct __attribute__((packed)) ReversePacket {
    IndicatorState indicatorState;
    uint8_t headlight: 1;
    uint8_t brake: 1;
    uint8_t running: 1;
    uint8_t starter: 1;
};
