#ifndef SERIAL_PACKET_FUNCTIONS_H
#define SERIAL_PACKET_FUNCTIONS_H

#include <Arduino.h>

#define PACKET_START 0xAA
#define PACKET_MAX 64

// Function to send a packet
void sendPacket(HardwareSerial &serial, uint8_t type, uint8_t *data, uint8_t len);

// Call this in loop() to read incoming bytes
void updatePacket(HardwareSerial &serial, void (*handler)(uint8_t type, uint8_t *data, uint8_t len));

#endif