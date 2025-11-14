#include "BME280.hpp"
#include <iostream>
#include <unistd.h>

int main() {
    BME280 sensor;

    if (!sensor.begin()) {
        std::cerr << "Failed to initialize BME280 sensor!" << std::endl;
        return 1;
    }

    while (true) {
        float temp, press, hum;
        sensor.readData(temp, press, hum);

        std::cout << "Temperature: " << temp << " °C\n";
        std::cout << "Pressure: " << press << " hPa\n";
        std::cout << "Humidity: " << hum << " %\n\n";

        sleep(2); // Wait 2 seconds
    }

    return 0;
}
