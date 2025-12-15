#include <gtest/gtest.h>
#include "BME280.hpp"
#include <cmath>

/*
  IMPORTANT:
  - Do NOT define UNIT_TEST macros here
  - They are already provided by CMake
*/

class BME280Test : public ::testing::Test {
protected:
    BME280 sensor;

    void SetUp() override {
        ASSERT_TRUE(sensor.begin());
    }
};

TEST_F(BME280Test, BeginSucceeds) {
    // If begin() failed, SetUp() would already fail
    SUCCEED();
}

TEST_F(BME280Test, ReadDataProducesFiniteValues) {
    float temperature = 0.0f;
    float pressure    = 0.0f;
    float humidity    = 0.0f;

    sensor.readData(temperature, pressure, humidity);

    EXPECT_TRUE(std::isfinite(temperature));
    EXPECT_TRUE(std::isfinite(pressure));
    EXPECT_TRUE(std::isfinite(humidity));
}

TEST_F(BME280Test, TemperatureIsInReasonableRange) {
    float t, p, h;
    sensor.readData(t, p, h);

    EXPECT_GT(t, -40.0f);   // BME280 minimum
    EXPECT_LT(t,  85.0f);   // BME280 maximum
}

TEST_F(BME280Test, PressureIsInReasonableRange) {
    float t, p, h;
    sensor.readData(t, p, h);

    EXPECT_GT(p, 300.0f);   // hPa
    EXPECT_LT(p, 1100.0f);  // hPa
}

TEST_F(BME280Test, HumidityIsInValidRange) {
    float t, p, h;
    sensor.readData(t, p, h);

    EXPECT_GE(h, 0.0f);
    EXPECT_LE(h, 100.0f);
}
