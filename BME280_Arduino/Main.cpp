#include <Arduino.h>
#include "BME280Sensor.hpp"

BME280Sensor bmeSensor;

void setup() {
  Serial.begin(9600);
  
  // Check if the BME280 sensor is connected
  if (!bmeSensor.begin()) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
}

void loop() {
  // Read temperature, pressure, and humidity from the sensor
  float temperature, pressure, humidity;
  bmeSensor.readData(temperature, pressure, humidity);

  // Print the sensor data
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" *C");

  Serial.print("Pressure: ");
  Serial.print(pressure);
  Serial.println(" hPa");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  // Wait for a short delay before reading the sensor again
  delay(2000);
}
