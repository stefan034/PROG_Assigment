#pragma once
#include <atomic>
#include <cstdint>
#include <string>

#include "Roomba.hpp"
// Translates MQTT commands to Roomba actions.
//
// Topics (with prefix "roomba"):
//   roomba/cmd/mode           payload: passive | safe | full
//   roomba/cmd/action         payload: clean | max | spot | dock | power | reset
//   roomba/cmd/drive          payload: "<speed mm/s> <radius mm
//   roomba/cmd/drive_direct   payload: "<right mm/s> <left mm/s>"
//   roomba/cmd/motors         payload: "<main brush> <side brush> <vacuum>" (PWM)
//   roomba/cmd/stop           payload: (empty) -> stop wheels
//   roomba/cmd/leds           payload: "<led bits> <power color> <power intensity>"
//   roomba/cmd/sensors        payload: (empty) -> read sensors and publish on MQTT
//   
// Safety: if no drive command is received for longer than kDriveTimeoutMs while the robot is moving, tick() will automatically stop the wheels.

class CommandHandler {
public:
    static constexpr int64_t kDriveTimeoutMs = 1000;

    /// @brief  Make a CommandHandler that translates MQTT commands to Roomba actions.
    ///         The handler is trhead-sace, but the caller has to take care of the Roomba
    /// @param roomba // Reference to the Roomba object to control.
    /// @param prefix 
    CommandHandler(Roomba& roomba, std::string prefix);


    /// @brief  Handle an incoming MQTT message. The topic is expected to be of the form <prefix>/cmd/<command>.
    /// @param topic The MQTT topic of the message.
    /// @param payload The payload of the message.
    void handle(const std::string& topic, const std::string& payload);
    void tick();  // Periodically called from the main loop (watchdog)

private:
    /// @brief  Mark the last time a drive command was received. If no drive command is received for longer than kDriveTimeoutMs while the robot is moving, tick() will automatically stop the wheels.
    /// @param moving 
    void markDrive(bool moving);
    static int64_t nowMs(); // Return the current time in milliseconds since epoch.

    Roomba& roomba_;
    std::string cmdPrefix_;
    std::atomic<int64_t> lastDriveMs_{0};
    std::atomic<bool> moving_{false};
};