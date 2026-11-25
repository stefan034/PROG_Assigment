#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

// Small wrapper around a POSIX serial port (termios), 8N1, no flow control.
class SerialPort {
public:
    SerialPort() = default;
    ~SerialPort();
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    /// @brief Opens the serial port.
    /// @param device The device path (e.g., "/dev/ttyUSB0").
    /// @param baud The baud rate.
    /// @return True if the port was opened successfully, false otherwise.
    bool open(const std::string& device, int baud);

    void close();

    /// @brief  Checks if the serial port is open.
    /// @return True if the port is open, false otherwise.
    bool isOpen() const { return fd_ >= 0; }

    /// @brief Writes data to the serial port.
    /// @param data Pointer to the data to write.
    /// @param len Length of the data to write.
    /// @return True if the data was written successfully, false otherwise.
    bool write(const uint8_t* data, size_t len);

    /// @brief Reads exactly `len` bytes within `timeoutMs`.
    /// @param buf Pointer to the buffer to read into.
    /// @param len Length of the data to read.
    /// @param timeoutMs Timeout in milliseconds.
    /// @return True if the data was read successfully, false otherwise.
    bool readExact(uint8_t* buf, size_t len, int timeoutMs);

    void flushInput();

    /// @brief Reads a line of text until '\n' (without \r\n in `line`). A half-received line is kept buffered on timeout.
    /// @param line Reference to the string to read into.
    /// @param timeoutMs Timeout in milliseconds.
    /// @return True if the line was read successfully, false otherwise.
    bool readLine(std::string& line, int timeoutMs);

    const std::string& lastError() const { return lastError_; }

private:
    int fd_ = -1;
    std::string lastError_;
    std::string rxBuf_;
};