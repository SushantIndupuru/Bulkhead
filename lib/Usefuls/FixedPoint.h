#ifndef FIXEDPOINT_H
#define FIXEDPOINT_H

#include <Arduino.h>


//encode a float number like 12.34 into a fixed-point integer (hundredths)
uint16_t encodeNumberToFixed(float num);

//decode a fixed-point integer back to float
float decodeFixedToNumber(uint16_t encoded);


#endif
