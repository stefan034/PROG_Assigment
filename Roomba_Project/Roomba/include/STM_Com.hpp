#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include "MQTTClient.hpp"
#include "SerialPort.hpp"

// One sample from the BME280, in integer units as sent by the STM32.
struct BmeSample {
    int32_t tempCentiC = 0;    // 0.01 °C   (2345 = 23.45 °C)
    int32_t pressurePa = 0;    // Pa        (101325 = 1013.25 hPa)
    int32_t humCentiPct = 0;   // 0.01 %RH  (4520 = 45.20 %)
};

// Reads lines from the STM32 in a separate thread and publishes them as JSON on MQTT.
//
// Protocol (one line per measurement, 115200 baud 8N1):
//   $BME,<temp_centi_C>,<pressure_Pa>,<humidity_centi_pct>*<XOR-checksum in 2 hex digits>\r\n
//   $ERR,nodata*<checksum>\r\n        (sensor not reachable)
// The checksum is the XOR of all characters between '$' and '*'.
class BmeReader {
public:
    BmeReader(std::string device, int baud, MqttClient& mqtt, std::string topic);
    ~BmeReader();

    void start();
    void stop();

    /// @brief Parses a line of text into a BmeSample.
    /// @param line The line to parse.
    /// @param out The BmeSample to populate.
    /// @return True if the line was parsed successfully, false otherwise.
    static bool parseLine(const std::string& line, BmeSample& out);

    /// @brief Converts a BmeSample to a JSON string.
    /// @param s The BmeSample to convert.
    /// @return The JSON string representation of the BmeSample.
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