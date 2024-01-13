#include "BME280.hpp"
#include <iomanip>
#include <iostream>

int main() {
    const char *i2cDevice = "/dev/i2c-1";  // Change this based on your Raspberry Pi's configuration
    BME280 bme280(i2cDevice);

    if (!bme280.initialize()) {
        std::cerr << "Failed to initialize BME280 sensor." << std::endl;
        return 1;
    }

    float temperature = bme280.readTemperature();
    float pressure = bme280.readPressure();
    float humidity = bme280.readHumidity();

    std::cout << "Temperature: " << std::fixed << std::setprecision(2) << temperature << " °C" << std::endl;
    std::cout << "Pressure: " << std::fixed << std::setprecision(2) << pressure << " hPa" << std::endl;
    std::cout << "Humidity: " << std::fixed << std::setprecision(2) << humidity << " %" << std::endl;

    return 0;
}

