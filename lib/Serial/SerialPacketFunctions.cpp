#include "./SerialPacketFunctions.h"

static enum { WAIT_START, READ_LEN, READ_TYPE, READ_DATA, READ_CHECK } state = WAIT_START;
static uint8_t length, type, data[PACKET_MAX], index, checksum;

void sendPacket(HardwareSerial &serial, uint8_t type, uint8_t *payload, uint8_t len) {
    uint8_t cs = type ^ len;
    serial.write(PACKET_START);
    serial.write(len);
    serial.write(type);
    for (uint8_t i = 0; i < len; i++) {
        serial.write(payload[i]);
        cs ^= payload[i];
    }
    serial.write(cs);
}

void updatePacket(HardwareSerial &serial, void (*handler)(uint8_t type, uint8_t *data, uint8_t len)) {
    while (serial.available()) {
        uint8_t b = serial.read();
        switch(state) {
            case WAIT_START:
                if (b == PACKET_START) { state = READ_LEN; checksum = 0; }
                break;
            case READ_LEN:
                length = b;
                if (length > PACKET_MAX) state = WAIT_START;
                else { checksum ^= b; index = 0; state = READ_TYPE; }
                break;
            case READ_TYPE:
                type = b;
                checksum ^= b;
                state = READ_DATA;
                break;
            case READ_DATA:
                data[index++] = b;
                checksum ^= b;
                if (index >= length) state = READ_CHECK;
                break;
            case READ_CHECK:
                if (checksum == b && handler) handler(type, data, length);
                state = WAIT_START;
                break;
        }
    }
}