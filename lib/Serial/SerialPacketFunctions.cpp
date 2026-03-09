#include <Arduino.h>
#include "./SerialPacketFunctions.h"

constexpr size_t HISTORY_SIZE = 128;

static uint8_t history[HISTORY_SIZE];
static size_t historyLen = 0;

void updatePacket(HardwareSerial &serial, void (*handler)(uint8_t type, uint8_t *data, uint8_t len)) {
    while (serial.available() && historyLen < HISTORY_SIZE) {
        history[historyLen++] = serial.read();
    }

    if (historyLen < 4) return;

    bool found = false;
    size_t lastPacketEnd = 0;
    uint8_t latestType = 0;
    uint8_t latestData[PACKET_MAX];
    uint8_t latestLen = 0;

    size_t i = 0;
    while (i + 3 <= historyLen) {
        if (history[i] != PACKET_START) {
            i++;
            continue;
        }

        const uint8_t packetLen = history[i + 1];
        if (packetLen > PACKET_MAX) {
            i++;
            continue;
        }

        if (i + 3 + packetLen + 1 > historyLen) break; // incomplete packet

        const uint8_t packetType = history[i + 2];
        const uint8_t *packetData = &history[i + 3];
        const uint8_t packetChecksum = history[i + 3 + packetLen];

        uint8_t cs = packetLen ^ packetType;
        for (uint8_t j = 0; j < packetLen; j++) cs ^= packetData[j];

        if (cs == packetChecksum) {
            found = true;
            lastPacketEnd = i + 3 + packetLen + 1;
            latestType = packetType;
            latestLen = packetLen;
            memcpy(latestData, packetData, latestLen);
        }

        i += 3 + packetLen + 1;
    }

    if (found) {
        if (lastPacketEnd < historyLen) {
            memmove(history, history + lastPacketEnd, historyLen - lastPacketEnd);
            historyLen -= lastPacketEnd;
        } else {
            historyLen = 0;
        }

        if (handler) handler(latestType, latestData, latestLen);
    } else {
        size_t start = 0;
        while (start < historyLen && history[start] != PACKET_START) start++;
        if (start > 0) {
            memmove(history, history + start, historyLen - start);
            historyLen -= start;
        }

        if (historyLen == HISTORY_SIZE) {
            memmove(history, history + 1, HISTORY_SIZE - 1);
            historyLen--;
        }
    }
}

void sendPacket(HardwareSerial &serial, const uint8_t type, const uint8_t *data, const uint8_t len) {
    uint8_t cs = len ^ type;
    serial.write(PACKET_START);
    serial.write(len);
    serial.write(type);
    for (uint8_t i = 0; i < len; i++) {
        serial.write(data[i]);
        cs ^= data[i];
    }
    serial.write(cs);
}
