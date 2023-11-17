// main.cpp
#include <iostream>
#include "GY521_MPU6050.hpp"

int main() {
    GY521_MPU6050 mpu;

    if (!mpu.Initialize()) {
        std::cerr << "MPU-6050 initialization failed." << std::endl;
        return 1;
    }

    while (true) {
        int16_t accelX = mpu.ReadAccelerationX();
        std::cout << "Acceleration X: " << accelX << std::endl;
        bcm2835_delay(1000); // Add a delay if needed to control the data reading rate
    }

    return 0;
}
