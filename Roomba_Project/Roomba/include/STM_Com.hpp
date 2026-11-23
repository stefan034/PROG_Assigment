#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include "MQTTClient.hpp"
#include "SerialPort.hpp"

// Eén meting van de BME280, in gehele eenheden zoals de STM32 ze stuurt.
struct BmeSample {
    int32_t tempCentiC = 0;    // 0.01 °C   (2345 = 23.45 °C)
    int32_t pressurePa = 0;    // Pa        (101325 = 1013.25 hPa)
    int32_t humCentiPct = 0;   // 0.01 %RH  (4520 = 45.20 %)
};

// Leest regels van de STM32 in een eigen thread en publiceert ze als JSON op MQTT.
//
// Protocol (één regel per meting, 115200 baud 8N1):
//   $BME,<temp_centi_C>,<druk_Pa>,<vocht_centi_pct>*<XOR-checksum in 2 hex-cijfers>\r\n
//   $ERR,nodata*<checksum>\r\n        (sensor niet bereikbaar)
// De checksum is de XOR van alle tekens tussen '$' en '*'.
class BmeReader {
public:
    BmeReader(std::string device, int baud, MqttClient& mqtt, std::string topic);
    ~BmeReader();

    void start();
    void stop();

    static bool parseLine(const std::string& line, BmeSample& out);
    static std::string toJson(const BmeSample& s);

private:
    void run();

    std::string device_;
    int baud_;
    MqttClient& mqtt_;
    std::string topic_;
    SerialPort port_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};