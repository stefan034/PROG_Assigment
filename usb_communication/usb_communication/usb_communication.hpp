#ifndef USB_COMMUNICATION_HPP
#define USB_COMMUNICATION_HPP

#include <libusb.h>
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

#endif // USB_COMMUNICATION_HPP
