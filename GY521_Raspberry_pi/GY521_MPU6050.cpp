// GY521_MPU6050.cpp
#include "GY521_MPU6050.hpp"
#include <iostream>
#include <bcm2835.h>

GY521_MPU6050::GY521_MPU6050() {
    if (!bcm2835_init()) {
        std::cerr << "Failed to initialize bcm2835." << std::endl;
        exit(1);
    }

    bcm2835_i2c_begin();
    bcm2835_i2c_set_baudrate(10000); // Set the I2C baudrate to 10Kbps
    bcm2835_i2c_setSlaveAddress(0x68); // MPU-6050 I2C address (default)
}

GY521_MPU6050::~GY521_MPU6050() {
    bcm2835_i2c_end();
    bcm2835_close();
}

bool GY521_MPU6050::Initialize() {
    // Wake up the MPU-6050 (out of sleep mode)
    return bcm2835_i2c_writeReg8(0x6B, 0);
}

int16_t GY521_MPU6050::ReadAccelerationX() {
    char buf[2];
    bcm2835_i2c_read_register(0x3B, buf, sizeof(buf));
    int16_t accelX = (buf[0] << 8) | buf[1];
    return accelX;
}
