#include "CommandHandler.hpp"

#include <chrono>
#include <iostream>
#include <sstream>

CommandHandler::CommandHandler(Roomba& roomba, std::string prefix)
    : roomba_(roomba), cmdPrefix_(std::move(prefix) + "/cmd/") {}

int64_t CommandHandler::nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void CommandHandler::markDrive(bool moving) {
    lastDriveMs_ = nowMs();
    moving_ = moving;
}

void CommandHandler::handle(const std::string& topic, const std::string& payload) {
    if (topic.compare(0, cmdPrefix_.size(), cmdPrefix_) != 0) return;
    const std::string cmd = topic.substr(cmdPrefix_.size());
    std::istringstream in(payload);

    if (cmd == "mode") {
        if (payload == "passive") roomba_.passive();
        else if (payload == "safe") roomba_.safe();
        else if (payload == "full") roomba_.full();
        else std::cerr << "Onbekende modus: " << payload << "\n";
    } else if (cmd == "action") {
        if (payload == "clean") roomba_.clean();
        else if (payload == "max") roomba_.max();
        else if (payload == "spot") roomba_.spot();
        else if (payload == "dock") roomba_.seekDock();
        else if (payload == "power") roomba_.power();
        else if (payload == "reset") roomba_.reset();
        else std::cerr << "Onbekende actie: " << payload << "\n";
    } else if (cmd == "drive") {
        int velocity, radius;
        if (in >> velocity >> radius) {
            roomba_.drive(static_cast<int16_t>(velocity), static_cast<int16_t>(radius));
            markDrive(velocity != 0);
        } else {
            std::cerr << "Ongeldige drive payload: " << payload << "\n";
        }
    } else if (cmd == "drive_direct") {
        int right, left;
        if (in >> right >> left) {
            roomba_.driveDirect(static_cast<int16_t>(right), static_cast<int16_t>(left));
            markDrive(right != 0 || left != 0);
        } else {
            std::cerr << "Ongeldige drive_direct payload: " << payload << "\n";
        }
    } else if (cmd == "motors") {
        int mainBrush, sideBrush, vacuum;
        if (in >> mainBrush >> sideBrush >> vacuum) {
            roomba_.setMotorsPwm(static_cast<int8_t>(mainBrush), static_cast<int8_t>(sideBrush),
                                 static_cast<uint8_t>(vacuum));
        } else {
            std::cerr << "Ongeldige motors payload: " << payload << "\n";
        }
    } else if (cmd == "stop") {
        roomba_.stopMotors();
        markDrive(false);
    } else {
        std::cerr << "Onbekend commando: " << cmd << "\n";
    }
}

void CommandHandler::tick() {
    if (moving_ && nowMs() - lastDriveMs_ > kDriveTimeoutMs) {
        std::cerr << "Watchdog: geen drive-commando ontvangen, wielen gestopt\n";
        roomba_.stopMotors();
        moving_ = false;
    }
}