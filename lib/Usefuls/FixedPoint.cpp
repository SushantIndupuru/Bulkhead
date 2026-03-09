#include "FixedPoint.h"


uint16_t encodeNumberToFixed(float num) {
    if (num < 0.0f) num = 0.0f;
    if (num > 99.99f) num = 99.99f;
    return (uint16_t) (num * 100 + 0.5f);
}


float decodeFixedToNumber(uint16_t encoded) {
    return encoded / 100.0f; //division is more optimized compared to multiplication by 0.01f
}

