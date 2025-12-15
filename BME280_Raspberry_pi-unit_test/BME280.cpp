#include "BME280.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <iostream>
#include <cstring>

BME280::BME280(int i2c_bus, uint8_t address) : i2c_addr(address) {
    char filename[20];
    snprintf(filename, 19, "/dev/i2c-%d", i2c_bus);

#ifndef UNIT_TEST   // Real hardware only
    fd = open(filename, O_RDWR);
    if (fd < 0) perror("Failed to open I2C bus");

    if (ioctl(fd, I2C_SLAVE, i2c_addr) < 0)
        perror("Failed to connect to BME280");
#else
    // In unit tests we do not open /dev/i2c-X
    fd = -1;
#endif
}

BME280::~BME280() {
#ifndef UNIT_TEST
    if (fd >= 0) close(fd);
#endif
}

void BME280::writeReg(uint8_t reg, uint8_t value) {
#ifndef UNIT_TEST
    uint8_t buffer[2] = {reg, value};
    write(fd, buffer, 2);
#else
    // TEST MODE: pretend write succeeded
    (void)reg; (void)value;
#endif
}

uint8_t BME280::readReg(uint8_t reg) {
#ifndef UNIT_TEST
    write(fd, &reg, 1);
    uint8_t value;
    read(fd, &value, 1);
    return value;
#else
    // ---- UNIT TEST OVERRIDE ----
    if (reg == 0xD0) return 0x60;   // Pretend sensor chip ID always OK
    return 0;
#endif
}

void BME280::readRegs(uint8_t reg, uint8_t* buffer, uint8_t length) {
#ifndef UNIT_TEST
    write(fd, &reg, 1);
    read(fd, buffer, length);
#else
    // ---- UNIT TEST SIMULATED DATA ----
    memset(buffer, 0, length);

    // Fake calibration block
    if (reg == 0x88 && length == 26) {
#ifdef UNIT_TEST_FAKE_CAL
        buffer[0] = 0x50; buffer[1] = 0x6B;  // dig_T1
        buffer[2] = 0x33; buffer[3] = 0x67;  // dig_T2
        // rest left zero for simplicity
#endif
    }

    // Fake humidity calibration block
    if (reg == 0xE1 && length == 7) {
#ifdef UNIT_TEST_FAKE_CAL
        buffer[0] = 0x01;
        buffer[1] = 0x02;
        buffer[2] = 0x03;
#endif
    }

    // Fake measurement data
    if (reg == 0xF7 && length == 8) {
#ifdef UNIT_TEST_FAKE_MEASUREMENTS
        buffer[0] = 0x64; // P msb
        buffer[3] = 0x80; // T msb
        buffer[6] = 0x40; // H msb
#endif
    }

#endif
}

bool BME280::begin() {
    uint8_t id = readReg(0xD0);
    if (id != 0x60) return false;

#ifndef UNIT_TEST
    writeReg(0xF2, 0x01);
    writeReg(0xF4, 0x27);
    writeReg(0xF5, 0xA0);
#endif

    readCalibrationData();
    return true;
}

void BME280::readCalibrationData() {

#ifdef UNIT_TEST_FAKE_CAL
    // Fully deterministic calibration for repeatable unit tests
    dig_T1 = 27504;
    dig_T2 = 26435;
    dig_T3 = -1000;
    dig_P1 = 36477;
    dig_P2 = -10685;
    dig_H1 = 75;
    dig_H2 = 362;
    dig_H3 = 0;
    dig_H4 = 334;
    dig_H5 = 50;
    dig_H6 = 30;
    return;
#endif

    // Normal hardware path
    uint8_t buf[26];
    readRegs(0x88, buf, 26);

    dig_T1 = buf[1] << 8 | buf[0];
    dig_T2 = buf[3] << 8 | buf[2];
    dig_T3 = buf[5] << 8 | buf[4];

    dig_P1 = buf[7] << 8 | buf[6];
    dig_P2 = buf[9] << 8 | buf[8];
    dig_P3 = buf[11] << 8 | buf[10];
    dig_P4 = buf[13] << 8 | buf[12];
    dig_P5 = buf[15] << 8 | buf[14];
    dig_P6 = buf[17] << 8 | buf[16];
    dig_P7 = buf[19] << 8 | buf[18];
    dig_P8 = buf[21] << 8 | buf[20];
    dig_P9 = buf[23] << 8 | buf[22];

    dig_H1 = readReg(0xA1);

    uint8_t hbuf[7];
    readRegs(0xE1, hbuf, 7);

    dig_H2 = hbuf[1] << 8 | hbuf[0];
    dig_H3 = hbuf[2];
    dig_H4 = (int16_t)((hbuf[3] << 4) | (hbuf[4] & 0x0F));
    dig_H5 = (int16_t)((hbuf[5] << 4) | (hbuf[4] >> 4));
    dig_H6 = hbuf[6];
}

void BME280::readData(float &temperature, float &pressure, float &humidity) {

    uint8_t data[8];

#ifndef UNIT_TEST
    readRegs(0xF7, data, 8);
#else
    // Provide fully simulated measurement if enabled
#ifdef UNIT_TEST_FAKE_MEASUREMENTS
    data[0] = 0x64; data[1] = 0; data[2] = 0;
    data[3] = 0x80; data[4] = 0; data[5] = 0;
    data[6] = 0x40; data[7] = 0;
#else
    readRegs(0xF7, data, 8); // fall back to generic test stub
#endif
#endif

    int32_t adc_P = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
    int32_t adc_T = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
    int32_t adc_H = (data[6] << 8) | data[7];

    // ---- Temperature compensation ----
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) *
                      ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) *
                    ((int32_t)dig_T3)) >> 14;

    t_fine = var1 + var2;
    temperature = (t_fine * 5 + 128) >> 8;
    temperature /= 100.0;

    // ---- Pressure compensation ----
    int64_t p;
    int64_t var1p = ((int64_t)t_fine) - 128000;
    int64_t var2p = var1p * var1p * (int64_t)dig_P6;

    var2p = var2p + ((var1p * (int64_t)dig_P5) << 17);
    var2p = var2p + (((int64_t)dig_P4) << 35);

    var1p = ((var1p * var1p * (int64_t)dig_P3) >> 8) +
            ((var1p * (int64_t)dig_P2) << 12);

    var1p = (((((int64_t)1) << 47) + var1p) * ((int64_t)dig_P1)) >> 33;

    if (var1p == 0) {
        pressure = 0;
    } else {
        p = 1048576 - adc_P;
        p = (((p << 31) - var2p) * 3125) / var1p;

        var1p = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
        var2p = (((int64_t)dig_P8) * p) >> 19;

        p = ((p + var1p + var2p) >> 8) + (((int64_t)dig_P7) << 4);
        pressure = p / 25600.0;
    }

    // ---- Humidity ----
    int32_t v_x1 = t_fine - ((int32_t)76800);
    v_x1 = (((((adc_H << 14) - ((int32_t)dig_H4 << 20) -
               ((int32_t)dig_H5 * v_x1)) + 16384) >> 15) *
             (((((((v_x1 * (int32_t)dig_H6) >> 10) *
                  (((v_x1 * (int32_t)dig_H3) >> 11) + 32768)) >> 10) +
                2097152) * (int32_t)dig_H2 +
               8192) >> 14));

    v_x1 = v_x1 - (((((v_x1 >> 15) * (v_x1 >> 15)) >> 7) *
                    (int32_t)dig_H1) >> 4);

    v_x1 = (v_x1 < 0 ? 0 : v_x1);
    v_x1 = (v_x1 > 419430400 ? 419430400 : v_x1);

    humidity = (v_x1 >> 12) / 1024.0;
}
