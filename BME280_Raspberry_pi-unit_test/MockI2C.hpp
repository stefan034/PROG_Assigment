#include "I2CInterface.hpp"
#include <gmock/gmock.h>

class MockI2C : public I2CInterface {
public:
    MOCK_METHOD(bool, writeBytes, (const uint8_t* data, uint8_t len), (override));
    MOCK_METHOD(bool, readBytes, (uint8_t reg, uint8_t* buffer, uint8_t len), (override));
};
