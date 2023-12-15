#include "BME280.hpp"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

BME280::BME280(int bus, int address) {
    char filename[20];
    sprintf(filename, "/dev/i2c-%d", bus);

    if ((file = open(filename, O_RDWR)) < 0) {
        std::cerr << "Failed to open the i2c bus" << std::endl;
        exit(1);
    }

    if (ioctl(file, I2C_SLAVE, address) < 0) {
        std::cerr << "Failed to acquire bus access and/or talk to slave" << std::endl;
        exit(1);
    }

    bme280Address = address;
}

BME280::~BME280() {
    close(file);
}

uint16_t BME280::read16(int reg) {
    uint8_t buf[2];
    if (read(file, buf, 2) != 2) {
        std::cerr << "Failed to read from the i2c bus" << std::endl;
        exit(1);
    }
    return (uint16_t)(buf[0] | (buf[1] << 8));
}

uint32_t BME280::read24(int reg) {
    uint8_t buf[3];
    if (read(file, buf, 3) != 3) {
        std::cerr << "Failed to read from the i2c bus" << std::endl;
        exit(1);
    }
    return (uint32_t)(buf[0] | (buf[1] << 8) | (buf[2] << 16));
}

void BME280::write8(int reg, int value) {
    uint8_t buf[2] = {static_cast<uint8_t>(reg), static_cast<uint8_t>(value)};
    if (write(file, buf, 2) != 2) {
        std::cerr << "Failed to write to the i2c bus" << std::endl;
        exit(1);
    }
}

float BME280::readTemperature() {
   int32_t adc_T = read24(0xFA) >> 4;

    int32_t var1 = ((((adc_T >> 3) - ((int32_t)calibData.dig_T1 << 1))) *
                    ((int32_t)calibData.dig_T2)) >>
                   11;

    int32_t var2 = (((((adc_T >> 4) - ((int32_t)calibData.dig_T1)) *
                      ((adc_T >> 4) - ((int32_t)calibData.dig_T1))) >>
                     12) *
                    ((int32_t)calibData.dig_T3)) >>
                   14;

    t_fine = var1 + var2;

    return ((t_fine * 5 + 128) >> 8) / 100.0;
}

float BME280::readPressure() {
    int32_t adc_P = read24(0xF7) >> 4;

    int64_t var1, var2, p;

    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)calibData.dig_P6;
    var2 = var2 + ((var1 * (int64_t)calibData.dig_P5) << 17);
    var2 = var2 + (((int64_t)calibData.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)calibData.dig_P3) >> 8) +
           ((var1 * (int64_t)calibData.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calibData.dig_P1) >> 33;

    if (var1 == 0) {
        return 0; // avoid exception caused by division by zero
    }

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)calibData.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)calibData.dig_P8) * p) >> 19;

    p = ((p + var1 + var2) >> 8) + (((int64_t)calibData.dig_P7) << 4);

    return static_cast<float>(p) / 256.0;
}

float BME280::readHumidity() {
    int32_t adc_H = read16(0xFD);

    int32_t v_x1_u32r;

    v_x1_u32r = (t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)calibData.dig_H4) << 20) -
                     (((int32_t)calibData.dig_H5) * v_x1_u32r)) +
                    ((int32_t)16384)) >>
                   15) *
                  (((((((v_x1_u32r * ((int32_t)calibData.dig_H6)) >> 10) *
                       (((v_x1_u32r * ((int32_t)calibData.dig_H3)) >> 11) +
                        ((int32_t)32768))) >>
                      10) +
                     ((int32_t)2097152)) *
                    ((int32_t)calibData.dig_H2) +
                    8192) >>
                   14));

    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >>
                                7) *
                               ((int32_t)calibData.dig_H1)) >>
                              4));

    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
    v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);

    return static_cast<float>(v_x1_u32r >> 12) / 1024.0;
}
