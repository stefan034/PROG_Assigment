// usb_communication.cpp
#include "usb_communication.hpp"

USBCommunication::USBCommunication() : ctx(nullptr), handle(nullptr) {
    libusb_init(&ctx);
}

USBCommunication::~USBCommunication() {
    close();
    libusb_exit(ctx);
}

bool USBCommunication::open() {
    handle = libusb_open_device_with_vid_pid(nullptr, VENDOR_ID, PRODUCT_ID);
    if (handle == nullptr) {
        return false;
    }

    libusb_claim_interface(handle, 0);

    return true;
}

void USBCommunication::close() {
    if (handle != nullptr) {
        libusb_release_interface(handle, 0);
        libusb_close(handle);
        handle = nullptr;
    }
}

bool USBCommunication::sendCommand(const char* command) {
    if (handle == nullptr) {
        return false;
    }

    int transferred;
    int result = libusb_control_transfer(handle, 0x40, 0, 0, 0, (unsigned char*)command, std::strlen(command), 1000);
    return result == 0;
}