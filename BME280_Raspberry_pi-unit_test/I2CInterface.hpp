#ifndef I2C_INTERFACE_HPP
#define I2C_INTERFACE_HPP

#include <cstdint>

class I2CInterface {
public:
    virtual ~I2CInterface() {}

    virtual bool writeBytes(const uint8_t* data, uint8_t len) = 0;
    virtual bool readBytes(uint8_t reg, uint8_t* buffer, uint8_t len) = 0;
};

#endif
