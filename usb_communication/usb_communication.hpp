// usb_communication.h
#ifndef USB_COMMUNICATION_H
#define USB_COMMUNICATION_H

#include <libusb-1.0/libusb.h>
#include <cstring>

class USBCommunication {
public:
    USBCommunication();
    ~USBCommunication();

    bool open();
    void close();
    bool sendCommand(const char* command);

private:
    libusb_context* ctx;
    libusb_device_handle* handle;
};

#endif // USB_COMMUNICATION_H