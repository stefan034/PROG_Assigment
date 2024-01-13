#include "BME280.h"
#include <iostream>
#include <iomanip>
#include <fcntl.h>
#include <unistd.h>

#define I2C_SLAVE 0x0703
#define O_RDWR 02
#define BME280_I2C_ADDRESS 0x76
#define BME280_REGISTER_PRESSURE 0xF7
#define BME280_REGISTER_TEMPERATURE 0xFA
#define BME280_REGISTER_HUMIDITY 0xFD

// Define I2C_SLAVE constant
#ifndef I2C_SLAVE
#define I2C_SLAVE 0x0703
#endif

BME280::BME280(const char *i2cDevice) {
    this->i2cFile = -1;
    strncpy(this->i2cDevice, i2cDevice, sizeof(this->i2cDevice));
}

BME280::~BME280() {
    if (i2cFile != -1) {
        close(i2cFile);
    }
}

bool BME280::initialize() {
    i2cFile = open(i2cDevice, O_RDWR);
    if (i2cFile == -1) {
        std::cerr << "Error opening I2C device" << std::endl;
        return false;
    }

    // Alternative approach to set I2C address without direct ioctl include
    unsigned long i2c_address = BME280_I2C_ADDRESS;
    if (ioctl(i2cFile, I2C_SLAVE, i2c_address) < 0) {
        std::cerr << "Error setting I2C address" << std::endl;
        return false;
    }

    return true;
}

int BME280::readRegister(int reg) {
    unsigned char buf[1];
    buf[0] = static_cast<unsigned char>(reg);
    if (write(i2cFile, buf, 1) != 1) {
        std::cerr << "Error writing to I2C device" << std::endl;
        return -1;
    }

    if (read(i2cFile, buf, 1) != 1) {
        std::cerr << "Error reading from I2C device" << std::endl;
        return -1;
    }

    return buf[0];
}

void BME280::writeRegister(int reg, int value) {
    unsigned char buf[2];
    buf[0] = static_cast<unsigned char>(reg);
    buf[1] = static_cast<unsigned char>(value);

    if (write(i2cFile, buf, 2) != 2) {
        std::cerr << "Error writing to I2C device" << std::endl;
    }
}

short BME280::combineRegisters(unsigned char msb, unsigned char lsb) {
    return static_cast<short>((msb << 8) | lsb);
}

float BME280::readTemperature() {
    int msb = readRegister(BME280_REGISTER_TEMPERATURE);
    int lsb = readRegister(BME280_REGISTER_TEMPERATURE + 1);
    int xlsb = readRegister(BME280_REGISTER_TEMPERATURE + 2);

    int rawTemperature = (msb << 12) | (lsb << 4) | (xlsb >> 4);
    int var1, var2;
    var1 = ((((rawTemperature >> 3) - (temperatureCalibration.dig_T1 << 1))) *
            temperatureCalibration.dig_T2) >>
           11;
    var2 = (((((rawTemperature >> 4) - temperatureCalibration.dig_T1) *
              ((rawTemperature >> 4) - temperatureCalibration.dig_T1)) >>
             12) *
            temperatureCalibration.dig_T3) >>
           14;

    int t_fine = var1 + var2;
    float temperature = (t_fine * 5 + 128) >> 8;
    return temperature / 100.0f;
}

float BME280::readPressure() {
    int msb = readRegister(BME280_REGISTER_PRESSURE);
    int lsb = readRegister(BME280_REGISTER_PRESSURE + 1);
    int xlsb = readRegister(BME280_REGISTER_PRESSURE + 2);

    int rawPressure = (msb << 12) | (lsb << 4) | (xlsb >> 4);
    int var1, var2;
    var1 = (((temperatureFine) >> 1) - 64000);
    var2 = (((((rawPressure >> 4) - pressureCalibration.dig_P1) *
              ((rawPressure >> 4) - pressureCalibration.dig_P1)) >>
             12) *
            pressureCalibration.dig_P3) >>
           14;

    int t_fine = var1 + var2;
    float pressure = (rawPressure + (var1 + var2 + 32768) * pressureCalibration.dig_P4) >> 15;
    return pressure / 100.0f;
}

float BME280::readHumidity() {
    int msb = readRegister(BME280_REGISTER_HUMIDITY);
    int lsb = readRegister(BME280_REGISTER_HUMIDITY + 1);

    int rawHumidity = (msb << 8) | lsb;
    int var1 = (temperatureFine - 76800);
    var1 = (((((rawHumidity << 14) - (humidityCalibration.dig_H4 << 20) - (humidityCalibration.dig_H5 * var1)) +
              16384) >>
             15) *
            (((((((var1 * humidityCalibration.dig_H6) >> 10) *
                 (((var1 * humidityCalibration.dig_H3) >> 11) + 32768)) >>
                10) +
               2097152) *
              humidityCalibration.dig_H2 +
              8192) >>
             14));
    var1 = var1 - ((var1 >> 15) * ((var1 >> 15) + 1) / 2);
    var1 = (var1 < 0) ? 0 : var1;

    float humidity = static_cast<float>(var1) / 1024.0f;
    return humidity;
}
