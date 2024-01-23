// main.cpp
#include "usb_communication.hpp"
#include <iostream>
#include <unistd.h>

int main() {
    USBCommunication usbComm;

    if (!usbComm.open()) {
        std::cerr << "Failed to open USB device." << std::endl;
        return 1;
    }

    const char* commands[] = {"Command1", "Command2", "Command3"};

    for (const auto& command : commands) {
        if (!usbComm.sendCommand(command)) {
            std::cerr << "Failed to send command: " << command << std::endl;
        } else {
            std::cout << "Command sent successfully: " << command << std::endl;
        }

        // Introduce a delay for demonstration purposes
        usleep(1000000); // Sleep for 1 second
    }

    usbComm.close();

    return 0;
}