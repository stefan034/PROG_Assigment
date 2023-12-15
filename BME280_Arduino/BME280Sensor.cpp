#include "BME280Sensor.hpp"

BME280Sensor::BME280Sensor() {}

bool BME280Sensor::begin() {
  // Check if the BME280 sensor is connected
  return bme.begin(0x76);  // Adjust the I2C address if necessary
}

void BME280Sensor::readData(float &temperature, float &pressure, float &humidity) {
  // Read temperature, pressure, and humidity from the sensor
  temperature = bme.readTemperature();
  pressure = bme.readPressure() / 100.0F; // Convert pressure to hPa
  humidity = bme.readHumidity();
}
