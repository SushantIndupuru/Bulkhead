#include <Arduino.h>
#include <SerialPacketFunctions.h>
#include <Structs.h>
#include <FixedPoint.h>

//constants
constexpr float WHEEL_DIAMETER_METERS = 0.3f;
constexpr float POSITIVE_RESISTOR = 17100.0f;
constexpr float NEGATIVE_RESISTOR = 10000.0f;
constexpr unsigned long INDICATOR_INTERVAL = 350;
constexpr unsigned long FORWARD_PACKET_INTERVAL = 50;
constexpr uint16_t ENCODER_CPR = 360;

//hardware
constexpr uint8_t ENCODER_A = 2;
constexpr uint8_t ENCODER_B = 3;
constexpr uint8_t REAR_LEFT_BRAKE_LIGHT = 4;
constexpr uint8_t REAR_LEFT_RUN_LIGHT = 5;
constexpr uint8_t REAR_RIGHT_BRAKE_LIGHT = 6;
constexpr uint8_t REAR_RIGHT_RUN_LIGHT = 7;
constexpr uint8_t HEADLIGHT = 8;
constexpr uint8_t LEFT_INDICATOR = 9;
constexpr uint8_t RIGHT_INDICATOR = 10;
constexpr uint8_t STARTER = 11;
constexpr uint8_t VOLTAGE_SENSOR = A0;
constexpr uint8_t pins[] = {
    LEFT_INDICATOR, RIGHT_INDICATOR, HEADLIGHT,
    REAR_LEFT_RUN_LIGHT, REAR_LEFT_BRAKE_LIGHT, REAR_RIGHT_RUN_LIGHT, REAR_RIGHT_BRAKE_LIGHT
};

//lighting states
IndicatorState currentIndicatorState = INDICATOR_OFF;
bool brakeRequested = false;
bool runningRequested = false;

//speed variables
volatile unsigned long lastRiseTime = 0;
volatile unsigned long pulseInterval = 0;

//packet
ReversePacket latestReversePacket{};

inline void digitalWriteRelay(const uint8_t pin, const bool on) {
    digitalWrite(pin, on ? LOW : HIGH);
}

void setRearLED(const uint8_t brakePin, const uint8_t runPin, const bool brake, const bool run) {
    digitalWriteRelay(brakePin, brake);
    digitalWriteRelay(runPin, brake ? false : run);
}

void encoderRise() {
    const unsigned long now = micros();
    static unsigned long prev = 0;
    if (prev > 0) pulseInterval = now - prev;
    prev = now;
    lastRiseTime = now;
}

void handlePacket(const uint8_t type, const uint8_t *data, const uint8_t len) {
    if (type == 2 && len == sizeof(ReversePacket))
        memcpy(&latestReversePacket, data, sizeof(ReversePacket));
}

uint8_t getSpeed() {
    unsigned long interval, last;
    noInterrupts();
    interval = pulseInterval;
    last = lastRiseTime;
    interrupts();

    if (interval == 0 || micros() - last > 2000000UL) return 0;

    const float pulseFreq = 1000000.0f / static_cast<float>(interval); //pulses/sec
    const float wheelRPS = pulseFreq / ENCODER_CPR;
    const float ms = wheelRPS * (PI * WHEEL_DIAMETER_METERS);

    return min(static_cast<uint8_t>(ms * 2.237f), (uint8_t)255);
}

float getVoltage() {
    constexpr float vcc = 4.98f;
    constexpr float scale = (POSITIVE_RESISTOR + NEGATIVE_RESISTOR) / NEGATIVE_RESISTOR;
    static float filteredVoltage = 0.0f;

    uint32_t sum = 0;
    for (int i = 0; i < 8; i++) {
        sum += analogRead(VOLTAGE_SENSOR);
    }
    const float raw = static_cast<float>(sum) / 8.0f * (vcc / 1024.0f);

    const float voltage = raw * scale;
    constexpr float alpha = 0.15f;
    filteredVoltage = filteredVoltage + alpha * (voltage - filteredVoltage);

    return filteredVoltage;
}

void updateIndicators() {
    static bool indicatorBlinkState = false;
    static unsigned long lastIndicatorToggle = 0;

    const unsigned long now = millis();
    if (now - lastIndicatorToggle >= INDICATOR_INTERVAL) {
        lastIndicatorToggle = now;
        indicatorBlinkState = !indicatorBlinkState;
    }

    const bool blink = indicatorBlinkState;

    switch (currentIndicatorState) {
        case INDICATOR_OFF:
            digitalWriteRelay(LEFT_INDICATOR, false);
            digitalWriteRelay(RIGHT_INDICATOR, false);
            setRearLED(REAR_LEFT_BRAKE_LIGHT, REAR_LEFT_RUN_LIGHT, brakeRequested, runningRequested);
            setRearLED(REAR_RIGHT_BRAKE_LIGHT, REAR_RIGHT_RUN_LIGHT, brakeRequested, runningRequested);
            break;

        case LEFT:
            digitalWriteRelay(LEFT_INDICATOR, blink);
            digitalWriteRelay(RIGHT_INDICATOR, false);
            setRearLED(REAR_LEFT_BRAKE_LIGHT, REAR_LEFT_RUN_LIGHT, blink, runningRequested);
            setRearLED(REAR_RIGHT_BRAKE_LIGHT, REAR_RIGHT_RUN_LIGHT, brakeRequested, runningRequested);
            break;

        case RIGHT:
            digitalWriteRelay(LEFT_INDICATOR, false);
            digitalWriteRelay(RIGHT_INDICATOR, blink);
            setRearLED(REAR_LEFT_BRAKE_LIGHT, REAR_LEFT_RUN_LIGHT, brakeRequested, runningRequested);
            setRearLED(REAR_RIGHT_BRAKE_LIGHT, REAR_RIGHT_RUN_LIGHT, blink, runningRequested);
            break;

        case HAZARDS:
            digitalWriteRelay(LEFT_INDICATOR, blink);
            digitalWriteRelay(RIGHT_INDICATOR, blink);
            setRearLED(REAR_LEFT_BRAKE_LIGHT, REAR_LEFT_RUN_LIGHT, blink, runningRequested);
            setRearLED(REAR_RIGHT_BRAKE_LIGHT, REAR_RIGHT_RUN_LIGHT, blink, runningRequested);
            break;
    }
}

void setup() {
    Serial.begin(9600);
    for (const uint8_t pin: pins) {
        pinMode(pin, OUTPUT);
        digitalWriteRelay(pin, false);
    }
    pinMode(STARTER, OUTPUT);
    digitalWriteRelay(STARTER, false);
    pinMode(ENCODER_A, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoderRise, RISING);

    // Sequentially test LEDs on boot
    for (const uint8_t pin: pins) {
        digitalWriteRelay(pin, true);
        delay(300);
        digitalWriteRelay(pin, false);
        delay(150);
    }
}

void loop() {
    updatePacket(Serial, handlePacket);

    digitalWriteRelay(HEADLIGHT, latestReversePacket.headlight);
    brakeRequested = latestReversePacket.brake;
    runningRequested = latestReversePacket.running;
    currentIndicatorState = latestReversePacket.indicatorState;
    updateIndicators();

    digitalWriteRelay(STARTER, latestReversePacket.starter);

    static unsigned long lastForwardSend = 0;
    const unsigned long now = millis();
    if (now - lastForwardSend >= FORWARD_PACKET_INTERVAL) {
        lastForwardSend = now;
        ForwardPacket packet = {getSpeed(), encodeNumberToFixed(getVoltage())};
        sendPacket(Serial, 1, reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
    }
}
