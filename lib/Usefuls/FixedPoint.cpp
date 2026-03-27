#include "FixedPoint.h"


uint16_t encodeNumberToFixed(float num) {
    if (num < 0.0f) num = 0.0f;
    if (num > 99.99f) num = 99.99f;
    uint16_t val = static_cast<uint16_t>(num * 100.0f + 0.5f);
    if (val > 9999) val = 9999;
    return val;
}


float decodeFixedToNumber(const uint16_t encoded) {
    return encoded *0.01f; //multiplication is more optimized compared to dividng by 100.0f
}