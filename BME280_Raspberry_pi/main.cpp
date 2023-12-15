#include "BME280.hpp"
#include <iostream>

int main() {
    try {
        BME280 bme(1, 0x76); // Use 0x77 if your BME280 sensor has a different address

        float temperature = bme.readTemperature();
        float pressure = bme.readPressure();
        float humidity = bme.readHumidity();

        std::cout << "Temperature: " << temperature << " °C" << std::endl;
        std::cout << "Pressure: " << pressure << " hPa" << std::endl;
        std::cout << "Humidity: " << humidity << " %" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
