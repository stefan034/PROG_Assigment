#include "GY521_MPU6050.hpp"

int main() {
    GY521 gy521;

    if (!gy521.initialize()) {
        std::cerr << "Initialization failed." << std::endl;
        return 1;
    }

    while (true) {
        gy521.readSensorData();
        // Add a delay if needed
        usleep(1000000); // Delay for 1 second
    }

    return 0;
}
