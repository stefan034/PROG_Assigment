// GY521_MPU6050.hpp
#ifndef GY521_MPU6050_HPP
#define GY521_MPU6050_HPP

class GY521_MPU6050 {
public:
    GY521_MPU6050();
    ~GY521_MPU6050();

    bool Initialize();
    int16_t ReadAccelerationX();
};

#endif
