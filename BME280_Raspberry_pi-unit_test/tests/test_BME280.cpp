#include "gtest/gtest.h"
#include "BME280.hpp"

// Enable test-only simulation code paths
#define UNIT_TEST
#define UNIT_TEST_FAKE_CAL
#define UNIT_TEST_FAKE_MEASUREMENTS

// Create a new instance using test mode
class BME280_Test : public ::testing::Test {
protected:
    BME280* sensor;

    void SetUp() override {
        sensor = new BME280();
        ASSERT_TRUE(sensor->begin());
    }

    void TearDown() override {
        delete sensor;
    }
};

// ---------------------------------------------------------
// Test: begin() should return true (chip ID simulated = 0x60)
// ---------------------------------------------------------
TEST_F(BME280_Test, ChipID_OK) {
    BME280 localSensor;
    EXPECT_TRUE(localSensor.begin());
}

// ---------------------------------------------------------
// Test: Calibration constants loaded from UNIT_TEST_FAKE_CAL
// ---------------------------------------------------------
TEST_F(BME280_Test, CalibrationLoaded) {
    EXPECT_EQ(sensor->dig_T1, 27504);
    EXPECT_EQ(sensor->dig_T2, 26435);
    EXPECT_EQ(sensor->dig_T3, -1000);
    EXPECT_EQ(sensor->dig_P1, 36477);
    EXPECT_EQ(sensor->dig_P2, -10685);

    EXPECT_EQ(sensor->dig_H1, 75);
    EXPECT_EQ(sensor->dig_H2, 362);
    EXPECT_EQ(sensor->dig_H3, 0);
    EXPECT_EQ(sensor->dig_H4, 334);
    EXPECT_EQ(sensor->dig_H5, 50);
    EXPECT_EQ(sensor->dig_H6, 30);
}

// ---------------------------------------------------------
// Test: Measurement compensation
// Fake measurement from BME280.cpp:
//   Temperature raw = 0x80000
//   Pressure raw    = 0x64000
//   Humidity raw    = 0x4000
// ---------------------------------------------------------
TEST_F(BME280_Test, MeasurementCompensation) {
    float t, p, h;
    sensor->readData(t, p, h);

    // Rough expected ranges based on the compensation formulas
    EXPECT_GT(t, -40.0);
    EXPECT_LT(t, 85.0);

    EXPECT_GT(p, 200.0);
    EXPECT_LT(p, 1200.0);

    EXPECT_GE(h, 0.0);
    EXPECT_LE(h, 100.0);
}

// ---------------------------------------------------------
// Test: Temperature deterministic output
// ---------------------------------------------------------
TEST_F(BME280_Test, TemperatureDeterministic) {
    float t1, p1, h1;
    float t2, p2, h2;

    sensor->readData(t1, p1, h1);
    sensor->readData(t2, p2, h2);

    EXPECT_FLOAT_EQ(t1, t2);  // must be identical because values are static
}

// ---------------------------------------------------------
// Test: Humidity deterministic output
// ---------------------------------------------------------
TEST_F(BME280_Test, HumidityDeterministic) {
    float t1, p1, h1;
    float t2, p2, h2;

    sensor->readData(t1, p1, h1);
    sensor->readData(t2, p2, h2);

    EXPECT_FLOAT_EQ(h1, h2);
}

// ---------------------------------------------------------
// Test: Pre
