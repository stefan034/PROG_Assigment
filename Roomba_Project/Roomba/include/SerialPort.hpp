#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

// Dunne wrapper om een POSIX seriële poort (termios), 8N1, geen flow control.
class SerialPort {
public:
    SerialPort() = default;
    ~SerialPort();
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    bool open(const std::string& device, int baud);
    void close();
    bool isOpen() const { return fd_ >= 0; }

    bool write(const uint8_t* data, size_t len);
    // Leest exact `len` bytes binnen `timeoutMs`. Geeft false bij timeout/fout.
    bool readExact(uint8_t* buf, size_t len, int timeoutMs);
    void flushInput();
    // Leest tot en met '\n' (zonder \r\n in `line`). Een half ontvangen regel blijft bewaard bij timeout.
    bool readLine(std::string& line, int timeoutMs);

    const std::string& lastError() const { return lastError_; }

private:
    int fd_ = -1;
    std::string lastError_;
    std::string rxBuf_;
};