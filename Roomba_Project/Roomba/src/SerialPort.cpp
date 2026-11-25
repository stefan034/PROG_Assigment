/*
 * SerialPort: a small wrapper around a POSIX serial port (termios), used both for the
 * Roomba and for the STM32 with the BME280.
 *
 * Settings: 8 data bits, no parity, 1 stop bit, no flow control (8N1), "raw" mode so the
 * kernel does not change any bytes (no echo, no newline translation).
 *
 * Reading uses select() with a deadline, so a read never blocks forever: it either
 * delivers the requested data or fails with a timeout.
 *
 * Errors are reported through the return value (false) plus a text in lastError().
 * Note: BmeReader compares lastError() to "Timeout bij lezen" to tell a normal timeout
 * from a real failure, so do not change that text without updating BmeReader.
 */

#include "SerialPort.hpp"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

namespace {
// Converts a baud rate (e.g. 115200) to the constant that termios uses (B115200).
// termios does not accept plain numbers, only these predefined constants.
// Returns false for rates that are not in the list.
// 115200 is the Create 2 default, 19200 is the alternative that can be set on the robot.
bool toSpeed(int baud, speed_t& out) {
    switch (baud) {
        case 19200:  out = B19200;  return true;
        case 57600:  out = B57600;  return true;
        case 115200: out = B115200; return true;
        default: return false;
    }
}
}  // namespace

// The port is closed automatically when the object goes out of scope (RAII).
SerialPort::~SerialPort() { close(); }

// Opens and configures the port, e.g. open("/dev/ttyUSB0", 115200).
// Returns false on failure; the reason is available through lastError().
bool SerialPort::open(const std::string& device, int baud) {
    close();   // make sure a previously opened port is released first

    speed_t speed;
    if (!toSpeed(baud, speed)) {
        lastError_ = "Niet-ondersteunde baudrate";
        return false;
    }

    // O_RDWR: read and write. O_NOCTTY: do not let this device become our controlling
    // terminal (otherwise e.g. a line break could send signals to the program).
    fd_ = ::open(device.c_str(), O_RDWR | O_NOCTTY);
    if (fd_ < 0) {
        // Typical reasons: "Permission denied" (user not in the dialout group),
        // or "No such file or directory" (wrong device name).
        lastError_ = "open(" + device + "): " + std::strerror(errno);
        return false;
    }

    // Read the current settings, change what we need, and write them back.
    termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
        lastError_ = std::string("tcgetattr: ") + std::strerror(errno);
        close();
        return false;
    }
    cfmakeraw(&tty);  // raw mode: 8 data bits, no parity, no echo, no byte processing
    cfsetispeed(&tty, speed);   // input baud rate
    cfsetospeed(&tty, speed);   // output baud rate
    tty.c_cflag |= (CLOCAL | CREAD);      // ignore modem control lines, enable the receiver
    tty.c_cflag &= ~(CSTOPB | CRTSCTS);   // 1 stop bit, no hardware flow control

    // VMIN = 0 and VTIME = 0 make read() return immediately with whatever is available.
    // The waiting itself is done with select() in readExact(), so these stay at zero.
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    // TCSANOW = apply the settings immediately.
    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        lastError_ = std::string("tcsetattr: ") + std::strerror(errno);
        close();
        return false;
    }

    // Discard anything that was already in the input and output buffers.
    tcflush(fd_, TCIOFLUSH);
    return true;
}

// Closes the port and forgets any half-received line. Safe to call more than once.
void SerialPort::close() {
    rxBuf_.clear();
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

// Writes all `len` bytes. A single write() may send fewer bytes than requested,
// so it loops until everything is sent. EINTR means the call was interrupted by a
// signal (e.g. Ctrl+C handler) before writing anything; that is not an error, just retry.
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
    // Wait until all bytes have actually left the serial hardware, so the caller knows
    // the command is really on the wire when this function returns.
    tcdrain(fd_);
    return true;
}

// Reads exactly `len` bytes, or fails after `timeoutMs` milliseconds in total.
// The deadline is fixed at the start, so partial reads do not extend the total wait.
bool SerialPort::readExact(uint8_t* buf, size_t len, int timeoutMs) {
    if (fd_ < 0) return false;
    using clock = std::chrono::steady_clock;   // monotonic: not affected by clock changes
    const auto deadline = clock::now() + std::chrono::milliseconds(timeoutMs);
    size_t got = 0;
    while (got < len) {
        // How much time is left until the deadline?
        auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - clock::now());
        if (remaining.count() <= 0) {
            lastError_ = "Timeout bij lezen";
            return false;
        }

        // Prepare select() with the remaining time. It sleeps until data arrives
        // on the port or the time runs out, without wasting CPU in a busy loop.
        timeval tv{};
        tv.tv_sec = static_cast<time_t>(remaining.count() / 1000000);
        tv.tv_usec = static_cast<suseconds_t>(remaining.count() % 1000000);
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd_, &rfds);
        int r = select(fd_ + 1, &rfds, nullptr, nullptr, &tv);
        if (r < 0) {
            if (errno == EINTR) continue;   // interrupted by a signal: just try again
            lastError_ = std::string("select: ") + std::strerror(errno);
            return false;
        }
        if (r == 0) continue;  // select timed out; the deadline check at the top reports it

        // Data is available: read as much as we still need. read() may return fewer
        // bytes than requested, which is why this whole thing is a loop.
        ssize_t n = ::read(fd_, buf + got, len - got);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;   // harmless, retry
            lastError_ = std::string("read: ") + std::strerror(errno);
            return false;
        }
        got += static_cast<size_t>(n);
    }
    return true;
}

// Throws away everything that has been received but not yet read (the input buffer).
// Used before sending a request, so the answer cannot be mixed up with old bytes.
void SerialPort::flushInput() {
    if (fd_ >= 0) tcflush(fd_, TCIFLUSH);
}

// Reads one line, ending at '\n' (a '\r' before it is dropped), within `timeoutMs`.
// Used for the text lines sent by the STM32, e.g. "$BME,2345,101325,4520*5A".
//
// Bytes are collected in the member buffer `rxBuf_`. If the timeout hits halfway through
// a line, the part received so far is KEPT for the next call, so no data is lost
// when a line arrives in two pieces.
bool SerialPort::readLine(std::string& line, int timeoutMs) {
    using clock = std::chrono::steady_clock;
    const auto deadline = clock::now() + std::chrono::milliseconds(timeoutMs);
    while (true) {
        auto rem = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - clock::now()).count();
        if (rem <= 0) {
            lastError_ = "Timeout bij lezen";
            return false;
        }

        // Read a single byte, waiting at most for the time that is left.
        uint8_t c;
        if (!readExact(&c, 1, static_cast<int>(rem))) return false;

        if (c == '\n') {
            // End of line: hand the collected text to the caller and start fresh.
            line = rxBuf_;
            rxBuf_.clear();
            return true;
        }
        if (c != '\r') {   // skip carriage returns (the STM32 sends "\r\n")
            // Cap the buffer at 256 characters. A longer "line" means garbage
            // (e.g. wrong baud rate), so it is discarded instead of growing forever.
            if (rxBuf_.size() < 256) rxBuf_.push_back(static_cast<char>(c));
            else rxBuf_.clear();
        }
    }
}