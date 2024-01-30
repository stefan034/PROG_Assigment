// main.cpp
#include "BME280.hpp"
#include <iostream>

int main() {
    BME280 bme280;

    if (!bme280.init()) {
        std::cerr << "Error initializing BME280 sensor." << std::endl;
        return 1;
    }

    float temperature = bme280.getTemperature();
    float pressure = bme280.getPressure();
    float humidity = bme280.getHumidity();

    std::cout << "Temperature: " << temperature << " °C" << std::endl;
    std::cout << "Pressure: " << pressure << " hPa" << std::endl;
    std::cout << "Humidity: " << humidity << " %" << std::endl;

    return 0;
}
