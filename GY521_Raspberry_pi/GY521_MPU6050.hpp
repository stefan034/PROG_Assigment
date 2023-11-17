#ifndef GY521_MPU6050_HPP
#define GY521_MPU6050_HPP

class GY521 {
public:
    GY521();
    ~GY521();

    bool initialize();
    void readSensorData();

private:
    int i2c_fd; // File descriptor for I2C communication

    // MPU-6050 registers
    static const int GY521_ADDR = 0x68;
    static const int PWR_MGMT_1 = 0x6B;
    static const int ACCEL_XOUT_H = 0x3B;
    static const int ACCEL_YOUT_H = 0x3D;
    static const int ACCEL_ZOUT_H = 0x3F;
    static const int GYRO_XOUT_H = 0x43;
    static const int GYRO_YOUT_H = 0x45;
    static const int GYRO_ZOUT_H = 0x47;
};

#endif // GY521_HPP
