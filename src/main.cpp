#include <Arduino.h>
#include <SerialPacketFunctions.h>
#include <Structs.h>
#include <FixedPoint.h>

constexpr uint8_t LEFT_INDICATOR = 9;
constexpr uint8_t RIGHT_INDICATOR = 10;
constexpr uint8_t HEADLIGHT = 8;
constexpr uint8_t RUNNING = 11;
constexpr uint8_t REAR_LEFT_BRAKE_LIGHT = 6;
constexpr uint8_t REAR_RIGHT_BRAKE_LIGHT = 7;

const uint8_t pins[] = {
    LEFT_INDICATOR,
    RIGHT_INDICATOR,
    HEADLIGHT,
    RUNNING,
    REAR_LEFT_BRAKE_LIGHT,
    REAR_RIGHT_BRAKE_LIGHT
};

IndicatorState currentIndicatorState = INDICATOR_OFF;

bool indicatorBlinkState = false;
unsigned long lastIndicatorToggle = 0;
constexpr unsigned long INDICATOR_INTERVAL = 350;
bool leftBrakeLight = false;
bool rightBrakeLight = false;

ReversePacket latestReversePacket{};

void handlePacket(uint8_t type, uint8_t *data, uint8_t len) {
    if (type == 2 && len == sizeof(ReversePacket)) {
        memcpy(&latestReversePacket, data, sizeof(ReversePacket));
    }
}

uint8_t getSpeed() {
    return 0;
}

float getVoltage() {
    return 0;
}

void setHeadLights(bool state) {
    digitalWrite(HEADLIGHT, state);
}

void setIndicatorLights(IndicatorState state) {
    currentIndicatorState = state;
}

void setBrakeLights(bool state) {
    leftBrakeLight = state;
    rightBrakeLight = state;
}

void setRunningLights(bool state) {
    digitalWrite(RUNNING, state);
}

void updateIndicators() {
    unsigned long now = millis();

    if (now - lastIndicatorToggle >= INDICATOR_INTERVAL) {
        lastIndicatorToggle = now;
        indicatorBlinkState = !indicatorBlinkState;
    }

    switch (currentIndicatorState) {
        case INDICATOR_OFF:
            digitalWrite(LEFT_INDICATOR, LOW);
            digitalWrite(RIGHT_INDICATOR, LOW);
            break;

        case LEFT:
            digitalWrite(LEFT_INDICATOR, indicatorBlinkState);
            digitalWrite(RIGHT_INDICATOR, LOW);

            leftBrakeLight = indicatorBlinkState;
            break;

        case RIGHT:
            digitalWrite(RIGHT_INDICATOR, indicatorBlinkState);
            digitalWrite(LEFT_INDICATOR, LOW);

            rightBrakeLight = indicatorBlinkState;
            break;

        case HAZARDS:
            digitalWrite(LEFT_INDICATOR, indicatorBlinkState);
            digitalWrite(RIGHT_INDICATOR, indicatorBlinkState);

            leftBrakeLight = indicatorBlinkState;
            rightBrakeLight = indicatorBlinkState;
            break;
    }
}

void setup() {
    Serial.begin(115200);
    for (const uint8_t pin: pins) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
    //test leds
    for (const uint8_t pin: pins) {
        digitalWrite(pin, HIGH);
        delay(300);
        digitalWrite(pin, LOW);
        delay(150);
    }
}

void loop() {
    updatePacket(Serial, handlePacket);

    setHeadLights(latestReversePacket.headlight);
    setBrakeLights(latestReversePacket.brake);
    setRunningLights(latestReversePacket.running);
    setIndicatorLights(latestReversePacket.indicatorState);
    updateIndicators();
    digitalWrite(REAR_LEFT_BRAKE_LIGHT, leftBrakeLight);
    digitalWrite(REAR_RIGHT_BRAKE_LIGHT, rightBrakeLight);

    ForwardPacket packet = {getSpeed(), encodeNumberToFixed(getVoltage())};
    sendPacket(Serial, 1, reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
}
