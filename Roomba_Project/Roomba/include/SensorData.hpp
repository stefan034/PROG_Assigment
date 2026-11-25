#pragma once
#include <array>
#include <cstdint>
#include <string>

enum class OiMode : uint8_t { Off = 0, Passive = 1, Safe = 2, Full = 3, Unknown = 255 };

const char* toString(OiMode mode);
const char* chargingStateToString(uint8_t state);

// A snapshot of the sensors we poll.
struct SensorData {
    uint8_t bumpsAndDrops = 0;     // packet 7
    uint8_t chargingState = 0;     // packet 21
    uint16_t voltageMv = 0;        // packet 22
    int16_t currentMa = 0;         // packet 23
    int8_t temperatureC = 0;       // packet 24
    uint16_t chargeMah = 0;        // packet 25
    uint16_t capacityMah = 0;      // packet 26
    OiMode mode = OiMode::Unknown; // packet 35
    uint8_t lightBumper = 0;       // packet 45
    int16_t leftEncoder = 0;       // packet 43
    int16_t rightEncoder = 0;      // packet 44
    bool virtualWall = false;      // packet 13

    bool bumpRight() const { return bumpsAndDrops & 0x01; }
    bool bumpLeft() const { return bumpsAndDrops & 0x02; }
    bool wheelDropRight() const { return bumpsAndDrops & 0x04; }
    bool wheelDropLeft() const { return bumpsAndDrops & 0x08; }

    // Returns a JSON string (without external JSON library).
    std::string toJson() const;
};