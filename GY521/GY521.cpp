#include "GY521.h"

// MPU-6050 I2C address
#define MPU6050_ADDR 0x68

// MPU-6050 registers
#define MPU6050_REG_PWR_MGMT_1 0x6B
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_TEMP_OUT_H 0x41
#define MPU6050_REG_GYRO_XOUT_H 0x43

GY521::GY521() {}

void GY521::begin() {
    Wire.begin();
    
    // Power on the sensor
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(MPU6050_REG_PWR_MGMT_1);
    Wire.write(0);
    Wire.endTransmission(true);

    delay(100); // Let the sensor stabilize
}

int16_t GY521::readSensorWord(uint8_t reg) {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU6050_ADDR, 2, true);

    // Read high byte and low byte
    uint8_t highByte = Wire.read();
    uint8_t lowByte = Wire.read();

    // Combine high byte and low byte into a 16-bit signed value
    int16_t value = (highByte << 8) | lowByte;

    return value;
}

double GY521::getAccelerometerX() {
    int16_t accelX = readSensorWord(MPU6050_REG_ACCEL_XOUT_H);
    return accelX / 16384.0;
}

double GY521::getTemperature() {
    int16_t temp = readSensorWord(MPU6050_REG_TEMP_OUT_H);
    return (temp / 340.0) + 36.53;
}

double GY521::getGyroscopeX() {
    int16_t gyroX = readSensorWord(MPU6050_REG_GYRO_XOUT_H);
    return gyroX / 131.0;
}
