#pragma once
#include <cstdint>
#include <initializer_list>
#include <mutex>

#include "SensorData.hpp"
#include "SerialPort.hpp"


// Implement a part of the iRobot Create 2 / Roomba 600 Open Interface.
// All public methods are thread-safe (MQTT callbacks run in a different thread).
class Roomba {
public:
    static constexpr int16_t kRadiusStraight = 32767;  // 0x7FFF
    static constexpr int16_t kRadiusTurnCw = -1;
    static constexpr int16_t kRadiusTurnCcw = 1;

    explicit Roomba(SerialPort& port) : port_(port) {}

    // Modus-commando's
    bool start();    // opcode 128 -> Passive
    bool passive();  // same as start()
    bool safe();     // opcode 131
    bool full();     // opcode 132
    bool stop();     // opcode 173 -> OI off
    bool reset();    // opcode 7

    // Reinigingscommando's
    bool clean();    // 135
    bool max();      // 136
    bool spot();     // 134
    bool seekDock(); // 143
    bool power();    // 133

    // Actuatoren (only in Safe/Full)
    bool drive(int16_t velocityMmS, int16_t radiusMm);          // 137
    bool driveDirect(int16_t rightMmS, int16_t leftMmS);        // 145
    bool stopMotors();                                          // drive 0
    bool setMotorsPwm(int8_t mainBrush, int8_t sideBrush, uint8_t vacuum);  // 144
    bool setLeds(uint8_t ledBits, uint8_t powerColor, uint8_t powerIntensity);  // 139

    // Sensor readings are cached in `out` and can be accessed from other threads.
    bool readSensors(SensorData& out);

private:
    bool send(std::initializer_list<uint8_t> bytes);  // caller holds mutex 
    SerialPort& port_;
    std::mutex mutex_;
};