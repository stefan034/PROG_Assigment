#ifndef BME280_HPP
#define BME280_HPP

#include <cstdint>

class BME280 {
public:
    BME280(int i2c_bus = 1, uint8_t address = 0x76);
    ~BME280();

    bool begin();
    void readData(float &temperature, float &pressure, float &humidity);

private:
    int fd;  // File descriptor for I2C
    uint8_t i2c_addr;

    // Calibration data
    uint16_t dig_T1;
    int16_t dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    uint8_t  dig_H1;
    int16_t dig_H2, dig_H3;
    int8_t  dig_H6;
    uint8_t dig_H4, dig_H5;

    int32_t t_fine;

    void writeReg(uint8_t reg, uint8_t value);
    uint8_t readReg(uint8_t reg);
    void readRegs(uint8_t reg, uint8_t* buffer, uint8_t length);

    void readCalibrationData();
};

#endif
