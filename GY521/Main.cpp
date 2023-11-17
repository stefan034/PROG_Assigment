#include "GY521.h"
//#include <Wire.h>

GY521 gy521;

void setup() {
    Serial.begin(9600);
    gy521.begin();
}

void loop() {
    double accelX = gy521.getAccelerometerX();
    double temp = gy521.getTemperature();
    double gyroX = gy521.getGyroscopeX();

    Serial.print("Accelerometer X: ");
    Serial.println(accelX);
    Serial.print("Temperature: ");
    Serial.print(temp);
    Serial.println(" °C");
    Serial.print("Gyroscope X: ");
    Serial.println(gyroX);

    delay(1000);
}