/*
 * CommandHandler: translates incoming MQTT messages into Roomba actions.
 *
 * Topic layout (with prefix "roomba"):
 *   roomba/cmd/mode          payload: passive | safe | full
 *   roomba/cmd/action        payload: clean | max | spot | dock | power | reset
 *   roomba/cmd/drive         payload: "<velocity mm/s> <radius mm>"      e.g. "200 32767" (straight)
 *   roomba/cmd/drive_direct  payload: "<right mm/s> <left mm/s>"         e.g. "100 -100" (spin in place)
 *   roomba/cmd/motors        payload: "<main brush> <side brush> <vacuum>" (PWM values)
 *   roomba/cmd/stop          payload: empty -> stop the wheels
 *
 * Safety watchdog: if the robot is driving and no new drive command arrives within
 * kDriveTimeoutMs, tick() stops the wheels. This protects against a lost network
 * connection or a crashed controller leaving the robot driving by itself.
 *
 * Threading: handle() is called from mosquitto's network thread, tick() from the main loop.
 * The two timestamps/flags that both threads touch are std::atomic, and the Roomba class
 * protects its serial port with its own mutex.
 */

#include "CommandHandler.hpp"

#include <chrono>
#include <iostream>
#include <sstream>

// `prefix` is the topic prefix (e.g. "roomba"). We store it with "/cmd/" appended so that
// handle() can strip it off every incoming topic with a single compare.
// std::move avoids copying the string we just received by value.
CommandHandler::CommandHandler(Roomba& roomba, std::string prefix)
    : roomba_(roomba), cmdPrefix_(std::move(prefix) + "/cmd/") {}

// Current time in milliseconds on a monotonic clock.
// steady_clock never jumps backwards (unlike the system clock when NTP adjusts it),
// which makes it the right choice for measuring timeouts.
int64_t CommandHandler::nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Remembers when the last drive command arrived and whether the robot is (supposed to be) moving.
// The watchdog in tick() only acts while `moving_` is true, so a robot that stands still
// is never "stopped" unnecessarily.
void CommandHandler::markDrive(bool moving) {
    lastDriveMs_ = nowMs();
    moving_ = moving;
}

// Processes one incoming MQTT message.
// `topic` is the full topic (e.g. "roomba/cmd/drive"), `payload` the message text (e.g. "200 32767").
void CommandHandler::handle(const std::string& topic, const std::string& payload) {
    // Ignore anything that does not start with "<prefix>/cmd/".
    // compare(0, n, s) checks only the first n characters of the topic.
    if (topic.compare(0, cmdPrefix_.size(), cmdPrefix_) != 0) return;

    // The command name is whatever follows the prefix, e.g. "drive".
    const std::string cmd = topic.substr(cmdPrefix_.size());

    // Turns the payload into a stream so that numbers can be read with `in >> a >> b`.
    // It splits on whitespace, which is why the payloads are space-separated.
    std::istringstream in(payload);

    if (cmd == "mode") {
        // Switch the Open Interface mode. Only Safe and Full accept drive and motor commands.
        if (payload == "passive") roomba_.passive();
        else if (payload == "safe") roomba_.safe();
        else if (payload == "full") roomba_.full();
        else std::cerr << "Onbekende modus: " << payload << "\n";

    } else if (cmd == "action") {
        // Built-in behaviours of the Roomba itself (cleaning, docking, powering down).
        if (payload == "clean") roomba_.clean();
        else if (payload == "max") roomba_.max();
        else if (payload == "spot") roomba_.spot();
        else if (payload == "dock") roomba_.seekDock();
        else if (payload == "power") roomba_.power();
        else if (payload == "reset") roomba_.reset();
        else std::cerr << "Onbekende actie: " << payload << "\n";

    } else if (cmd == "drive") {
        // Drive with an average velocity and a turning radius.
        // Special radius values: 32767 = straight, 1 = spin counter-clockwise, -1 = spin clockwise.
        // Values are read as int first and then narrowed to int16_t, because that is what the
        // Open Interface expects (16-bit signed). Roomba::drive() clamps them to the legal range.
        int velocity, radius;
        if (in >> velocity >> radius) {   // true only if BOTH numbers were read successfully
            roomba_.drive(static_cast<int16_t>(velocity), static_cast<int16_t>(radius));
            markDrive(velocity != 0);     // velocity 0 counts as "not moving" for the watchdog
        } else {
            std::cerr << "Ongeldige drive payload: " << payload << "\n";
        }

    } else if (cmd == "drive_direct") {
        // Control each wheel separately, in mm/s. Positive = forward, negative = backward.
        int right, left;
        if (in >> right >> left) {
            roomba_.driveDirect(static_cast<int16_t>(right), static_cast<int16_t>(left));
            markDrive(right != 0 || left != 0);   // moving if at least one wheel turns
        } else {
            std::cerr << "Ongeldige drive_direct payload: " << payload << "\n";
        }

    } else if (cmd == "motors") {
        // PWM duty cycles for the main brush, side brush and vacuum.
        // The brushes accept negative values (reverse); the vacuum only runs forward,
        // which is why it is cast to an unsigned type.
        // Note: this does not touch the watchdog, because the brushes are not a driving hazard.
        int mainBrush, sideBrush, vacuum;
        if (in >> mainBrush >> sideBrush >> vacuum) {
            roomba_.setMotorsPwm(static_cast<int8_t>(mainBrush), static_cast<int8_t>(sideBrush),
                                 static_cast<uint8_t>(vacuum));
        } else {
            std::cerr << "Ongeldige motors payload: " << payload << "\n";
        }

    } else if (cmd == "stop") {
        // Emergency stop for the wheels. The payload is ignored, so an empty message works.
        roomba_.stopMotors();
        markDrive(false);

    } else {
        std::cerr << "Onbekend commando: " << cmd << "\n";
    }
}

// Safety watchdog, called regularly from the main loop (every `pollMs`).
// If the robot is moving and the last drive command is older than kDriveTimeoutMs,
// the wheels are stopped. To keep driving, a client must therefore keep resending its
// drive command (at least once per second).
void CommandHandler::tick() {
    if (moving_ && nowMs() - lastDriveMs_ > kDriveTimeoutMs) {
        std::cerr << "Watchdog: geen drive-commando ontvangen, wielen gestopt\n";
        roomba_.stopMotors();
        moving_ = false;   // so the watchdog does not fire again until the next drive command
    }
}