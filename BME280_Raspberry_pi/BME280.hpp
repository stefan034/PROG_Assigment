// BME280Library.h

#ifndef BME280_LIBRARY_H
#define BME280_LIBRARY_H
#include <stdint.h>

class BME280 {
public:
    BME280(int deviceAddress = 0x76);
    ~BME280();


    bool init();
    float getTemperature();
    float getPressure();
    float getHumidity();

private:
    int fd;
    int calib[24];

    void readCalibration();
    uint32_t readTemperature();
    uint32_t readPressure();
    uint32_t readHumidity();

    uint16_t readS16(uint8_t reg);
    uint8_t readByte(uint8_t reg);
    void writeByte(uint8_t reg, uint8_t value);
};

#endif // BME280_LIBRARY_H
