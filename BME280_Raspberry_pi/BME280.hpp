#ifndef BME280_HPP
#define BME280_HPP

//#include <cstdint>
#include <cstdint>

class BME280 {
public:
    BME280(int bus, int address);
    ~BME280();

    float readTemperature();
    float readPressure();
    float readHumidity();

private:
    int file;
    int bme280Address;

    uint16_t read16(int reg);
    uint32_t read24(int reg);
    void write8(int reg, int value);
    int32_t t_fine;
};

#endif // BME280_HPP
