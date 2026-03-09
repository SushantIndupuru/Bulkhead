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
constexpr uint8_t SPED_SENSOR = A0;

constexpr float WHEEL_DIAMETER_METERS = 0.3f;  //TODO: measure actual wheel
constexpr int THRESHOLD_HIGH = 600; //TODO: tune
constexpr int THRESHOLD_LOW  = 400;

constexpr float POSITIVE_RESISTOR = 1500.0f;
constexpr float NEGATIVE_RESISTOR = 1000.0f;

constexpr unsigned long INDICATOR_INTERVAL = 350;
constexpr unsigned long FORWARD_PACKET_INTERVAL = 50; // ~20Hz

constexpr uint8_t pins[] = {
    LEFT_INDICATOR,
    RIGHT_INDICATOR,
    HEADLIGHT,
    RUNNING,
    REAR_LEFT_BRAKE_LIGHT,
    REAR_RIGHT_BRAKE_LIGHT
};

bool sensorLastState = false;
unsigned long lastRiseTime = 0;
unsigned long pulseInterval = 0;

IndicatorState currentIndicatorState = INDICATOR_OFF;

bool indicatorBlinkState = false;
unsigned long lastIndicatorToggle = 0;

bool brakeRequested = false;

ReversePacket latestReversePacket{};

void handlePacket(uint8_t type, uint8_t *data, uint8_t len) {
    if (type == 2 && len == sizeof(ReversePacket)) {
        memcpy(&latestReversePacket, data, sizeof(ReversePacket));
    }
}

void updateSpeedSensor() {
    const int val = analogRead(SPED_SENSOR);
    bool currentState = sensorLastState;

    if (!sensorLastState && val > THRESHOLD_HIGH) currentState = true;
    else if (sensorLastState && val < THRESHOLD_LOW) currentState = false;

    if (currentState && !sensorLastState) {
        unsigned long now = micros();
        if (lastRiseTime > 0) pulseInterval = now - lastRiseTime;
        lastRiseTime = now;
    }
    sensorLastState = currentState;
}

uint8_t getSpeed() {
    if (pulseInterval == 0) return 0;
    if (micros() - lastRiseTime > 2000000UL) return 0;

    const float rpm = (60.0f * 1000000.0f) / static_cast<float>(pulseInterval);
    const float ms  = (rpm / 60.0f) * (PI * WHEEL_DIAMETER_METERS);
    const auto mph = static_cast<uint8_t>(ms * 2.237f);
    return min(mph, (uint8_t)255);
}

float readVCC() {
    //measure actual supply voltage using internal 1.1V reference
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
    delay(2);
    ADCSRA |= _BV(ADSC);
    while (bit_is_set(ADCSRA, ADSC)) {}
    long result = ADCW;
    return (1.1f * 1023.0f * 1000.0f) / static_cast<float>(result) / 1000.0f;
}

float getVoltage() {
    const float vcc = readVCC();
    const int raw = analogRead(SPED_SENSOR);
    const float voltageAtPin = raw * (vcc / 1023);
    return voltageAtPin * ((POSITIVE_RESISTOR + NEGATIVE_RESISTOR) / NEGATIVE_RESISTOR);
}

void setHeadLights(bool state) {
    digitalWrite(HEADLIGHT, state);
}

void setRunningLights(bool state) {
    digitalWrite(RUNNING, state);
}

void setBrakeLights(bool state) {
    brakeRequested = state;
}

void setIndicatorLights(IndicatorState state) {
    currentIndicatorState = state;
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
            digitalWrite(REAR_LEFT_BRAKE_LIGHT, brakeRequested);
            digitalWrite(REAR_RIGHT_BRAKE_LIGHT, brakeRequested);
            break;

        case LEFT:
            digitalWrite(LEFT_INDICATOR, indicatorBlinkState);
            digitalWrite(RIGHT_INDICATOR, LOW);
            digitalWrite(REAR_LEFT_BRAKE_LIGHT, indicatorBlinkState);
            digitalWrite(REAR_RIGHT_BRAKE_LIGHT, brakeRequested);
            break;

        case RIGHT:
            digitalWrite(LEFT_INDICATOR, LOW);
            digitalWrite(RIGHT_INDICATOR, indicatorBlinkState);
            digitalWrite(REAR_LEFT_BRAKE_LIGHT, brakeRequested);
            digitalWrite(REAR_RIGHT_BRAKE_LIGHT, indicatorBlinkState);
            break;

        case HAZARDS:
            digitalWrite(LEFT_INDICATOR, indicatorBlinkState);
            digitalWrite(RIGHT_INDICATOR, indicatorBlinkState);
            digitalWrite(REAR_LEFT_BRAKE_LIGHT, indicatorBlinkState);
            digitalWrite(REAR_RIGHT_BRAKE_LIGHT, indicatorBlinkState);
            break;
    }
}

void setup() {
    Serial.begin(115200);
    for (const uint8_t pin: pins) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
    // Test LEDs sequentially on boot
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
    setRunningLights(latestReversePacket.running);
    setBrakeLights(latestReversePacket.brake);
    setIndicatorLights(latestReversePacket.indicatorState);
    updateIndicators();
    updateSpeedSensor();

    static unsigned long lastForwardSend = 0;
    unsigned long now = millis();
    if (now - lastForwardSend >= FORWARD_PACKET_INTERVAL) {
        lastForwardSend = now;
        ForwardPacket packet = {getSpeed(), encodeNumberToFixed(getVoltage())};
        sendPacket(Serial, 1, reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
    }
}
