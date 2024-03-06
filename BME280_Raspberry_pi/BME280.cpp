// BME280Library.cpp

#include "BME280.hpp"
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstdio>
#include <stdint.h>

#define BME280_REG_DIG_T1  0x88 
#define BME280_REG_DIG_T2  0x8A
#define BME280_REG_DIG_T3  0x8C

#define BME280_REG_DIG_P1  0x8E
#define BME280_REG_DIG_P2  0x90
#define BME280_REG_DIG_P3  0x92
#define BME280_REG_DIG_P4  0x94
#define BME280_REG_DIG_P5  0x96
#define BME280_REG_DIG_P6  0x98
#define BME280_REG_DIG_P7  0x9A
#define BME280_REG_DIG_P8  0x9C
#define BME280_REG_DIG_P9  0x9E

#define BME280_REG_DIG_H1  0xA1
#define BME280_REG_DIG_H2  0xE1
#define BME280_REG_DIG_H3  0xE3
#define BME280_REG_DIG_H4  0xE4
#define BME280_REG_DIG_H5  0xE5
#define BME280_REG_DIG_H6  0xE7

#define BME280_REG_CHIPID  0xD0
#define BME280_REG_RESET   0xE0
#define BME280_REG_CTRL_HUM 0xF2
#define BME280_REG_STATUS  0xF3
#define BME280_REG_CTRL_MEAS 0xF4
#define BME280_REG_CONFIG  0xF5
#define BME280_REG_PRESS_MSB  0xF7
#define BME280_REG_PRESS_LSB  0xF8
#define BME280_REG_PRESS_XLSB 0xF9
#define BME280_REG_TEMP_MSB   0xFA
#define BME280_REG_TEMP_LSB   0xFB
#define BME280_REG_TEMP_XLSB  0xFC
#define BME280_REG_HUM_MSB    0xFD
#define BME280_REG_HUM_LSB    0xFE

uint16_t BME280::readS16(uint8_t reg){
    uint8_t msb = readByte(reg);
    uint8_t lsb = readByte(reg + 1);
    uint16_t rawValue = (msb << 8) | lsb; 
    return rawValue;
}

uint8_t BME280::readByte(uint8_t reg) {
    if (write(fd, &reg, 1) != 1) {
        fprintf(stderr, "Error writing to register 0x%02X.\n", reg);
        return 0;
    }
    // value hoort hier niet 
    uint8_t value;
    if (read(fd, &value, 1) != 1) {
        fprintf(stderr, "Error reading from register 0x%02X.\n", reg);
        return 0;
    }

    return value;
}

void BME280::writeByte(uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    if (write(fd, data, 2) != 2) {
        fprintf(stderr, "Error writing to register 0x%02X.\n", reg);
    }
}

BME280::BME280(int deviceAddress) {
    char filename[20];
    sprintf(filename, "/dev/i2c-1");
    fd = open(filename, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Error: Unable to open I2C device.\n");
    } else {
        if (ioctl(fd, I2C_SLAVE, deviceAddress) < 0) {
            fprintf(stderr, "Error: Unable to set I2C device address.\n");
            close(fd);
        }
    }
}

BME280::~BME280() {
    close(fd);
}


bool BME280::init() {
    char chipID = readByte(BME280_REG_CHIPID);
    //fprintf(stderr, "BME280 Chip ID: 0x%X\n", chipID); // Print chip ID
    if (chipID != 0x60) {
        fprintf(stderr, "Error: Unable to find BME280 sensor.\n");
        return false;
    }

    readCalibration();

    // Set configuration
    writeByte(BME280_REG_CTRL_HUM, 0x05);  // Set humidity oversampling to x1 
    writeByte(BME280_REG_CTRL_MEAS, 0xB7); // Set temperature and pressure oversampling to x16 

    return true;
}

uint32_t BME280::readTemperature() {
    // Read raw temperature data from the sensor
    uint8_t msb = readByte(BME280_REG_TEMP_MSB);
    uint8_t lsb = readByte(BME280_REG_TEMP_LSB);
    uint8_t xlsb = readByte(BME280_REG_TEMP_XLSB);

    // Combine the 3 bytes into a 20-bit raw temperature value
    int32_t rawTemperature = (static_cast<int32_t>(msb) << 12) |
                              (static_cast<int32_t>(lsb) << 4) |
                              (static_cast<int32_t>(xlsb) >> 4);

    return rawTemperature;
}


float BME280::getTemperature() {
    int32_t tempRaw = readTemperature();
    int32_t var1; 
    int32_t var2;
    int32_t temperature;

    var1 = ((((tempRaw >> 3) - (calib[0] << 1))) * (calib[1])) >> 11;
    var2 = (((((tempRaw >> 4) - (calib[2])) * ((tempRaw >> 4) - (calib[2]))) >> 12) * (calib[3])) >> 14;
    temperature = var1 + var2;

    return static_cast<float>((temperature * 5 + 128) >> 8); /// </100.0f>
}

uint32_t BME280::readPressure() {
    // Read raw pressure data from the sensor
    uint8_t msb = readByte(BME280_REG_PRESS_MSB);
    uint8_t lsb = readByte(BME280_REG_PRESS_LSB);
    uint8_t xlsb = readByte(BME280_REG_PRESS_XLSB);

    // Combine the 3 bytes into a 20-bit raw pressure value
    uint32_t rawPressure = (static_cast<uint32_t>(msb) << 12) |
                           (static_cast<uint32_t>(lsb) << 4) |
                           (static_cast<uint32_t>(xlsb) >> 4);

    return rawPressure;
}

float BME280::getPressure() {
    int32_t pressureRaw = readPressure();
    int64_t var1, var2, p;

    var1 = ((int64_t)calib[4] * (((int64_t)calib[11] * (pressureRaw >> 15)) >> 24)) >> 8;
    var2 = (((int64_t)pressureRaw) >> 12 - ((int64_t)calib[10]));
    p = ((var1 + var2) * 281474976710656LL) >> 44;

    return static_cast<float>(p) / 256.0f;
}

uint32_t BME280::readHumidity() {
    // Read raw humidity data from the sensor
    uint8_t msb = readByte(BME280_REG_HUM_MSB);
    uint8_t lsb = readByte(BME280_REG_HUM_LSB);

    // Combine the 2 bytes into a 16-bit raw humidity value
    uint16_t rawHumidity = (static_cast<uint16_t>(msb) << 8) | lsb;

    return rawHumidity;
}

float BME280::getHumidity() {
    int32_t humidityRaw = readHumidity();
    int32_t var1, var2, humidity;

    var1 = (calib[6] * 16 + ((humidityRaw >> 4) - calib[7] * ((humidityRaw >> 4) + 128) >> 12)) * (calib[8] >> 11);
    var2 = (((humidityRaw >> 4) - calib[7]) * ((humidityRaw >> 4) - calib[7]) >> 12) * (calib[9] >> 11);
    humidity = var1 + var2;

    return static_cast<float>((humidity * 10 + 128) >> 8) / 10.0f;
}

void BME280::readCalibration() {
    calib[0] = readS16(BME280_REG_DIG_T1);
    calib[1] = readS16(BME280_REG_DIG_T2);
    calib[2] = readS16(BME280_REG_DIG_T3);

    calib[3] = readS16(BME280_REG_DIG_P1);
    calib[4] = readS16(BME280_REG_DIG_P2);
    calib[5] = readS16(BME280_REG_DIG_P3);
    calib[6] = readS16(BME280_REG_DIG_P4);
    calib[7] = readS16(BME280_REG_DIG_P5);
    calib[8] = readS16(BME280_REG_DIG_P6);
    calib[9] = readS16(BME280_REG_DIG_P7);
}
