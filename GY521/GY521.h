#ifndef GY521_h
#define GY521_h

#include <Wire.h>

class GY521 {
public:
    GY521();
    void begin();
    double getAccelerometerX();
    double getTemperature();
    double getGyroscopeX();

private:
    int16_t readSensorWord(uint8_t reg);
};

#endif
