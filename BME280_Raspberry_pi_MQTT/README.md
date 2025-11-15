Overview

This project reads temperature, humidity, and pressure data from a BME280 sensor and publishes the readings to an MQTT broker using the libmosquitto library.

Requirements

Raspberry Pi with I2C enabled

BME280 sensor connected to I2C pins

libmosquitto-dev installed

CMake and g++ installed

Installation
sudo apt update
sudo apt install cmake g++ libmosquitto-dev

Build
mkdir build
cd build
cmake ..
make

Run
sudo ./bme280_mqtt


(I2C typically requires root permissions unless udev rules are adjusted.)

Configuration

You can change MQTT settings in mqtt_config.h:
    Broker address
    Port
    Topic
    QoS
    Publish interval (ms)

Notes
Ensure your BME280 uses I2C address 0x76 or 0x77

You can verify I2C address with:
sudo i2cdetect -y 1

Future Improvements:
    TLS authentication
    Offline message buffering
    Multi-topic publishing
    Home Assistant autodiscovery