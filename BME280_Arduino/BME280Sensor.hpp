#ifndef BME280_SENSOR_HPP
#define BME280_SENSOR_HPP

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

class BME280Sensor {
public:
  BME280Sensor();
  bool begin();
  void readData(float &temperature, float &pressure, float &humidity);

private:
  Adafruit_BME280 bme;
};

#endif  // BME280_SENSOR_HPP
