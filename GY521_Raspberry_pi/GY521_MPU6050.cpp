#include "gy521.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <iostream>

GY521::GY521() : i2c_fd(-1) {
}

GY521::~GY521() {
    // Close the I2C interface
    if (i2c_fd != -1) {
        close(i2c_fd);
    }
}

bool GY521::initialize() {
    // Open the I2C interface to the GY-521
    i2c_fd = open("/dev/i2c-1", O_RDWR);

    if (i2c_fd == -1) {
        std::cerr << "Unable to open I2C interface." << std::endl;
        return false;
    }

    // Set the I2C slave address
    if (ioctl(i2c_fd, I2C_SLAVE, GY521_ADDR) < 0) {
        std::cerr << "Unable to set I2C slave address." << std::endl;
        return false;
    }

    // Initialize the GY-521 (Power Management Register)
    char buf[2] = {PWR_MGMT_1, 0};
    if (write(i2c_fd, buf, 2) != 2) {
        std::cerr << "Error writing to the device." << std::endl;
        return false;
    }

    return true;
}

void GY521::readSensorData() {
    // Read accelerometer data
    char buf[6];
    if (read(i2c_fd, buf, 6) != 6) {
        std::cerr << "Error reading from the device." << std::endl;
        return;
    }

    int accel_x = (buf[0] << 8) | buf[1];
    int accel_y = (buf[2] << 8) | buf[3];
    int accel_z = (buf[4] << 8) | buf[5];

    // Read gyroscope data
    if (read(i2c_fd, buf, 6) != 6) {
        std::cerr << "Error reading from the device." << std::endl;
        return;
    }

    int gyro_x = (buf[0] << 8) | buf[1];
    int gyro_y = (buf[2] << 8) | buf[3];
    int gyro_z = (buf[4] << 8) | buf[5];

    // Print the sensor values
    std::cout << "Accelerometer: X=" << accel_x << " Y=" << accel_y << " Z=" << accel_z << std::endl;
    std::cout << "Gyroscope: X=" << gyro_x << " Y=" << gyro_y << " Z=" << gyro_z << std::endl;
}
