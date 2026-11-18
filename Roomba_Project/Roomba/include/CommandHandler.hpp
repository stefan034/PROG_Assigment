#pragma once
#include <atomic>
#include <cstdint>
#include <string>

#include "Roomba.hpp"

// Vertaalt MQTT-commando's naar Roomba-acties.
//
// Topics (met prefix "roomba"):
//   roomba/cmd/mode           payload: passive | safe | full
//   roomba/cmd/action         payload: clean | max | spot | dock | power | reset
//   roomba/cmd/drive          payload: "<snelheid mm/s> <radius mm>"   bv. "200 32767" (rechtdoor)
//   roomba/cmd/drive_direct   payload: "<rechts mm/s> <links mm/s>"    bv. "100 -100"
//   roomba/cmd/motors         payload: "<hoofdborstel> <zijborstel> <stofzuiger>" (PWM)
//   roomba/cmd/stop           payload: (leeg) -> wielen stoppen
//
// Veiligheid: als er langer dan kDriveTimeoutMs geen drive-commando binnenkomt
// terwijl de robot rijdt, stopt tick() de wielen automatisch.
class CommandHandler {
public:
    static constexpr int64_t kDriveTimeoutMs = 1000;

    CommandHandler(Roomba& roomba, std::string prefix);

    void handle(const std::string& topic, const std::string& payload);
    void tick();  // periodiek aanroepen vanuit de hoofdlus (watchdog)

private:
    void markDrive(bool moving);
    static int64_t nowMs();

    Roomba& roomba_;
    std::string cmdPrefix_;
    std::atomic<int64_t> lastDriveMs_{0};
    std::atomic<bool> moving_{false};
};