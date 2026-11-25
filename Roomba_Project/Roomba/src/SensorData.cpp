/*
 * SensorData: helper functions for turning the raw Roomba sensor values into text.
 *
 * - toString(OiMode) and chargingStateToString() convert numeric codes from the Open
 *   Interface into readable strings.
 * - SensorData::toJson() builds the JSON message that is published on MQTT
 *   (topic <prefix>/sensors).
 *
 * The JSON is built by hand with a string stream, so no external JSON library is needed.
 * That is fine here because all values are numbers, booleans or fixed strings, so nothing
 * has to be escaped.
 */

#include "SensorData.hpp"

#include <sstream>

// Converts the OI mode (packet 35) to text. The switch covers all known modes;
// `default` catches OiMode::Unknown and any value that is not in the enum.
const char* toString(OiMode mode) {
    switch (mode) {
        case OiMode::Off: return "off";
        case OiMode::Passive: return "passive";
        case OiMode::Safe: return "safe";
        case OiMode::Full: return "full";
        default: return "unknown";
    }
}

// Converts the charging state code (packet 21, values 0..5 in the OI spec) to text.
// The returned pointers point to string literals, so they stay valid forever
// and the caller does not have to free anything.
const char* chargingStateToString(uint8_t state) {
    switch (state) {
        case 0: return "not_charging";
        case 1: return "reconditioning";   // slow charge for a deeply discharged battery
        case 2: return "full_charging";
        case 3: return "trickle_charging"; // battery is (nearly) full, maintenance charge
        case 4: return "waiting";
        case 5: return "fault";
        default: return "unknown";
    }
}

// Builds the JSON string for one sensor snapshot, for example:
// {"mode":"safe","charging_state":"not_charging","battery":{...},"bumper":{...}, ...}
std::string SensorData::toJson() const {
    // Small lambda that converts a bool to the JSON words true/false.
    // The `lightBumper & 0x01` calls below pass an int, which is implicitly
    // converted to bool: non-zero (bit set) = true.
    auto b = [](bool v) { return v ? "true" : "false"; };

    // Battery percentage = charge / capacity. The 100.0 makes it a floating-point division
    // (integer division would round down to 0 for almost every value). The ternary guards
    // against dividing by zero if the capacity has not been reported (yet).
    const int pct = capacityMah > 0 ? static_cast<int>(100.0 * chargeMah / capacityMah) : 0;

    // ostringstream collects all pieces and turns them into one string at the end.
    // Escaped quotes (\") are the quotes that belong to the JSON itself.
    std::ostringstream o;
    o << "{"
      << "\"mode\":\"" << toString(mode) << "\","
      << "\"charging_state\":\"" << chargingStateToString(chargingState) << "\","
      // Battery block. temperatureC is a signed 8-bit value; streaming an int8_t directly
      // would print it as a CHARACTER, so it is cast to int first to get a number.
      << "\"battery\":{"
      << "\"voltage_mv\":" << voltageMv << ","
      << "\"current_ma\":" << currentMa << ","        // negative = discharging
      << "\"temperature_c\":" << static_cast<int>(temperatureC) << ","
      << "\"charge_mah\":" << chargeMah << ","
      << "\"capacity_mah\":" << capacityMah << ","
      << "\"percent\":" << pct << "},"
      // Physical bumpers and wheel drop sensors (decoded from packet 7 by the helper methods).
      << "\"bumper\":{\"left\":" << b(bumpLeft()) << ",\"right\":" << b(bumpRight()) << "},"
      << "\"wheel_drop\":{\"left\":" << b(wheelDropLeft()) << ",\"right\":" << b(wheelDropRight()) << "},"
      // Light bumper (packet 45): one bit per infrared sensor, from the left to the right
      // side of the robot. The hex values are the bit masks of those bits.
      << "\"light_bumper\":{"
      << "\"left\":" << b(lightBumper & 0x01) << ","
      << "\"front_left\":" << b(lightBumper & 0x02) << ","
      << "\"center_left\":" << b(lightBumper & 0x04) << ","
      << "\"center_right\":" << b(lightBumper & 0x08) << ","
      << "\"front_right\":" << b(lightBumper & 0x10) << ","
      << "\"right\":" << b(lightBumper & 0x20) << "},"
      // Raw encoder counts (packets 43 and 44). These wrap around at +/-32768, so use
      // the DIFFERENCE between two readings when calculating distance.
      << "\"encoders\":{\"left\":" << leftEncoder << ",\"right\":" << rightEncoder << "},"
      << "\"virtual_wall\":" << b(virtualWall)
      << "}";
    return o.str();
}