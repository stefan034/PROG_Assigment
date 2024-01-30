// BME280Library.h

#ifndef BME280_LIBRARY_H
#define BME280_LIBRARY_H

class BME280 {
public:
    BME280(int deviceAddress = 0x76);
    ~BME280();

    bool init();
    float getTemperature();
    float getPressure();
    float getHumidity();

private:
    int fd;
    int calib[24];

    void readCalibration();
    uint32_t readTemperature();
    uint32_t readPressure();
    uint32_t readHumidity();
};

#endif // BME280_LIBRARY_H
