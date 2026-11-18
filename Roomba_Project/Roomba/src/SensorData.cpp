#include "sensor_data.hpp"

#include <sstream>

const char* toString(OiMode mode) {
    switch (mode) {
        case OiMode::Off: return "off";
        case OiMode::Passive: return "passive";
        case OiMode::Safe: return "safe";
        case OiMode::Full: return "full";
        default: return "unknown";
    }
}

const char* chargingStateToString(uint8_t state) {
    switch (state) {
        case 0: return "not_charging";
        case 1: return "reconditioning";
        case 2: return "full_charging";
        case 3: return "trickle_charging";
        case 4: return "waiting";
        case 5: return "fault";
        default: return "unknown";
    }
}

std::string SensorData::toJson() const {
    auto b = [](bool v) { return v ? "true" : "false"; };
    const int pct = capacityMah > 0 ? static_cast<int>(100.0 * chargeMah / capacityMah) : 0;

    std::ostringstream o;
    o << "{"
      << "\"mode\":\"" << toString(mode) << "\","
      << "\"charging_state\":\"" << chargingStateToString(chargingState) << "\","
      << "\"battery\":{"
      << "\"voltage_mv\":" << voltageMv << ","
      << "\"current_ma\":" << currentMa << ","
      << "\"temperature_c\":" << static_cast<int>(temperatureC) << ","
      << "\"charge_mah\":" << chargeMah << ","
      << "\"capacity_mah\":" << capacityMah << ","
      << "\"percent\":" << pct << "},"
      << "\"bumper\":{\"left\":" << b(bumpLeft()) << ",\"right\":" << b(bumpRight()) << "},"
      << "\"wheel_drop\":{\"left\":" << b(wheelDropLeft()) << ",\"right\":" << b(wheelDropRight()) << "},"
      << "\"light_bumper\":{"
      << "\"left\":" << b(lightBumper & 0x01) << ","
      << "\"front_left\":" << b(lightBumper & 0x02) << ","
      << "\"center_left\":" << b(lightBumper & 0x04) << ","
      << "\"center_right\":" << b(lightBumper & 0x08) << ","
      << "\"front_right\":" << b(lightBumper & 0x10) << ","
      << "\"right\":" << b(lightBumper & 0x20) << "},"
      << "\"encoders\":{\"left\":" << leftEncoder << ",\"right\":" << rightEncoder << "},"
      << "\"virtual_wall\":" << b(virtualWall)
      << "}";
    return o.str();
}