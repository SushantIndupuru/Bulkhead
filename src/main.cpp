#include <Arduino.h>
#include <SerialPacketFunctions.h>
#include <Structs.h>
#include <FixedPoint.h>

constexpr uint8_t LEFT_INDICATOR = 9;
constexpr uint8_t RIGHT_INDICATOR = 10;
constexpr uint8_t HEADLIGHT = 8;
constexpr uint8_t REAR_LEFT_RUN_LIGHT = 11;
constexpr uint8_t REAR_RIGHT_RUN_LIGHT = 5;
constexpr uint8_t REAR_LEFT_BRAKE_LIGHT = 6;
constexpr uint8_t REAR_RIGHT_BRAKE_LIGHT = 7;
constexpr uint8_t VOLTAGE_SENSOR = A0;
constexpr uint8_t ENCODER_A = 2;

constexpr float WHEEL_DIAMETER_METERS = 0.3f;
constexpr uint16_t THRESHOLD_HIGH = 600;
constexpr uint16_t THRESHOLD_LOW = 400;
constexpr float POSITIVE_RESISTOR = 1500.0f;
constexpr float NEGATIVE_RESISTOR = 1000.0f;
constexpr unsigned long INDICATOR_INTERVAL = 350;
constexpr unsigned long FORWARD_PACKET_INTERVAL = 50;

constexpr uint16_t ENCODER_CPR = 360;
constexpr uint8_t pins[] = {
    LEFT_INDICATOR, RIGHT_INDICATOR, HEADLIGHT,
    REAR_LEFT_RUN_LIGHT, REAR_LEFT_BRAKE_LIGHT, REAR_RIGHT_RUN_LIGHT, REAR_RIGHT_BRAKE_LIGHT
};

inline void setLED(uint8_t pin, bool on) {
    digitalWrite(pin, on ? LOW : HIGH);
}

void setRearLED(uint8_t brakePin, uint8_t runPin, bool brake, bool run) {
    setLED(brakePin, brake);
    setLED(runPin, brake ? false : run);
}

volatile unsigned long lastRiseTime = 0;
volatile unsigned long pulseInterval = 0;

IndicatorState currentIndicatorState = INDICATOR_OFF;
bool indicatorBlinkState = false;
unsigned long lastIndicatorToggle = 0;
bool brakeRequested = false;
bool runningRequested = false;
ReversePacket latestReversePacket{};

void tachRise() {
    unsigned long now = micros();
    static unsigned long prev = 0;
    if (prev > 0) pulseInterval = now - prev;
    prev = now;
    lastRiseTime = now;
}

void handlePacket(uint8_t type, uint8_t *data, uint8_t len) {
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
    const float vcc = 4.98f;
    const float scale = ((POSITIVE_RESISTOR + NEGATIVE_RESISTOR) / NEGATIVE_RESISTOR);
    static float filteredVoltage = 0.0f;

    uint32_t sum = 0;
    for (int i = 0; i < 8; i++) {
        sum += analogRead(VOLTAGE_SENSOR);
    }
    float raw = (sum / 8.0f) * (vcc / 1023.0f);

    float voltage = raw * scale;
    const float alpha = 0.15f;
    filteredVoltage = filteredVoltage + alpha * (voltage - filteredVoltage);

    return filteredVoltage;
}

void updateIndicators() {
    unsigned long now = millis();
    if (now - lastIndicatorToggle >= INDICATOR_INTERVAL) {
        lastIndicatorToggle = now;
        indicatorBlinkState = !indicatorBlinkState;
    }

    const bool blink = indicatorBlinkState;

    switch (currentIndicatorState) {
        case INDICATOR_OFF:
            setLED(LEFT_INDICATOR, false);
            setLED(RIGHT_INDICATOR, false);
            setRearLED(REAR_LEFT_BRAKE_LIGHT, REAR_LEFT_RUN_LIGHT, brakeRequested, runningRequested);
            setRearLED(REAR_RIGHT_BRAKE_LIGHT, REAR_RIGHT_RUN_LIGHT, brakeRequested, runningRequested);
            break;

        case LEFT:
            setLED(LEFT_INDICATOR, blink);
            setLED(RIGHT_INDICATOR, false);
            setRearLED(REAR_LEFT_BRAKE_LIGHT, REAR_LEFT_RUN_LIGHT, blink, runningRequested);
            setRearLED(REAR_RIGHT_BRAKE_LIGHT, REAR_RIGHT_RUN_LIGHT, brakeRequested, runningRequested);
            break;

        case RIGHT:
            setLED(LEFT_INDICATOR, false);
            setLED(RIGHT_INDICATOR, blink);
            setRearLED(REAR_LEFT_BRAKE_LIGHT, REAR_LEFT_RUN_LIGHT, brakeRequested, runningRequested);
            setRearLED(REAR_RIGHT_BRAKE_LIGHT, REAR_RIGHT_RUN_LIGHT, blink, runningRequested);
            break;

        case HAZARDS:
            setLED(LEFT_INDICATOR, blink);
            setLED(RIGHT_INDICATOR, blink);
            setRearLED(REAR_LEFT_BRAKE_LIGHT, REAR_LEFT_RUN_LIGHT, blink, runningRequested);
            setRearLED(REAR_RIGHT_BRAKE_LIGHT, REAR_RIGHT_RUN_LIGHT, blink, runningRequested);
            break;
    }
}

void setup() {
    Serial.begin(9600);
    for (const uint8_t pin: pins) {
        pinMode(pin, OUTPUT);
        setLED(pin, false);
    }
    pinMode(ENCODER_A, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(ENCODER_A), tachRise, RISING);

    // Sequentially test LEDs on boot
    for (const uint8_t pin: pins) {
        setLED(pin, true);
        delay(300);
        setLED(pin, false);
        delay(150);
    }
}

void loop() {
    updatePacket(Serial, handlePacket);

    setLED(HEADLIGHT, latestReversePacket.headlight);
    brakeRequested = latestReversePacket.brake;
    runningRequested = latestReversePacket.running;
    currentIndicatorState = latestReversePacket.indicatorState;
    updateIndicators();

    static unsigned long lastForwardSend = 0;
    unsigned long now = millis();
    if (now - lastForwardSend >= FORWARD_PACKET_INTERVAL) {
        lastForwardSend = now;
        ForwardPacket packet = {getSpeed(), encodeNumberToFixed(getVoltage())};
        sendPacket(Serial, 1, reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
    }
}
