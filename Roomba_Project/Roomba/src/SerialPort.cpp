#include "SerialPort.hpp"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

namespace {
bool toSpeed(int baud, speed_t& out) {
    switch (baud) {
        case 19200:  out = B19200;  return true;
        case 57600:  out = B57600;  return true;
        case 115200: out = B115200; return true;
        default: return false;
    }
}
}  // namespace

SerialPort::~SerialPort() { close(); }

bool SerialPort::open(const std::string& device, int baud) {
    close();
    speed_t speed;
    if (!toSpeed(baud, speed)) {
        lastError_ = "Niet-ondersteunde baudrate";
        return false;
    }
    fd_ = ::open(device.c_str(), O_RDWR | O_NOCTTY);
    if (fd_ < 0) {
        lastError_ = "open(" + device + "): " + std::strerror(errno);
        return false;
    }
    termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
        lastError_ = std::string("tcgetattr: ") + std::strerror(errno);
        close();
        return false;
    }
    cfmakeraw(&tty);  // 8 databits, geen pariteit, geen echo/verwerking
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(CSTOPB | CRTSCTS);  // 1 stopbit, geen hardware flow control
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;
    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        lastError_ = std::string("tcsetattr: ") + std::strerror(errno);
        close();
        return false;
    }
    tcflush(fd_, TCIOFLUSH);
    return true;
}

void SerialPort::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool SerialPort::write(const uint8_t* data, size_t len) {
    if (fd_ < 0) return false;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::write(fd_, data + sent, len - sent);
        if (n < 0) {
            if (errno == EINTR) continue;
            lastError_ = std::string("write: ") + std::strerror(errno);
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    tcdrain(fd_);
    return true;
}

bool SerialPort::readExact(uint8_t* buf, size_t len, int timeoutMs) {
    if (fd_ < 0) return false;
    using clock = std::chrono::steady_clock;
    const auto deadline = clock::now() + std::chrono::milliseconds(timeoutMs);
    size_t got = 0;
    while (got < len) {
        auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - clock::now());
        if (remaining.count() <= 0) {
            lastError_ = "Timeout bij lezen";
            return false;
        }
        timeval tv{};
        tv.tv_sec = static_cast<time_t>(remaining.count() / 1000000);
        tv.tv_usec = static_cast<suseconds_t>(remaining.count() % 1000000);
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd_, &rfds);
        int r = select(fd_ + 1, &rfds, nullptr, nullptr, &tv);
        if (r < 0) {
            if (errno == EINTR) continue;
            lastError_ = std::string("select: ") + std::strerror(errno);
            return false;
        }
        if (r == 0) continue;  // timeout wordt bovenaan afgehandeld
        ssize_t n = ::read(fd_, buf + got, len - got);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            lastError_ = std::string("read: ") + std::strerror(errno);
            return false;
        }
        got += static_cast<size_t>(n);
    }
    return true;
}

void SerialPort::flushInput() {
    if (fd_ >= 0) tcflush(fd_, TCIFLUSH);
}