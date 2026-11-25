/*
 * BmeReader: receives BME280 measurements from the STM32 over a serial port and publishes
 * them to MQTT as JSON.
 *
 * The STM32 sends one text line per measurement, for example:
 *     $BME,2345,101325,4520*5A\r\n
 * with the fields
 *     temperature in 0.01 degrees C   (2345   = 23.45 C)
 *     pressure in Pa                  (101325 = 1013.25 hPa)
 *     humidity in 0.01 %RH            (4520   = 45.20 %)
 * and, after the '*', a checksum (XOR of all characters between '$' and '*') as 2 hex digits.
 * Whole numbers are used so the STM32 does not need float printf support.
 *
 * If the sensor cannot be read, the STM32 sends "$ERR,nodata*CS" instead.
 *
 * The reader runs in its own thread, so the main loop (Roomba polling) is not blocked
 * while waiting for the next line.
 */

#include "STM_Com.hpp"

#include <chrono>
#include <cstdio>
#include <iostream>

namespace {
// Formats a value in hundredths as a decimal string with two decimals.
// v is in hundredths: 2345 -> "23.45", -305 -> "-3.05".
// Integer math is used on purpose: it avoids floating-point rounding surprises
// (e.g. 23.449999) and gives exactly the digits the STM32 sent.
// The sign is handled separately, because for -5 the integer parts would be 0 and -5,
// which would print as "0.-5" instead of "-0.05". The int64_t avoids overflow when
// negating the most negative int32_t value.
std::string fixed2(int32_t v) {
    char buf[32];
    const char* sign = v < 0 ? "-" : "";
    const int64_t a = v < 0 ? -static_cast<int64_t>(v) : v;
    std::snprintf(buf, sizeof(buf), "%s%d.%02d", sign, static_cast<int>(a / 100), static_cast<int>(a % 100));
    return buf;
}
}  // namespace

// Stores the settings. The reader does not open the port yet; that happens in the thread.
// `mqtt` is a reference to the shared MQTT client (it must outlive this object).
// std::move avoids copying the strings that were passed in by value.
BmeReader::BmeReader(std::string device, int baud, MqttClient& mqtt, std::string topic)
    : device_(std::move(device)), baud_(baud), mqtt_(mqtt), topic_(std::move(topic)) {}

// Makes sure the thread is stopped before the object disappears. Destroying a std::thread
// that is still running would terminate the whole program.
BmeReader::~BmeReader() { stop(); }

// Starts the background thread that runs run(). Calling it twice does nothing.
void BmeReader::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&BmeReader::run, this);
}

// Asks the thread to stop and waits until it has finished (join).
// running_ is atomic, so it can safely be changed from another thread.
// The thread notices the change within about a second (the readLine timeout).
void BmeReader::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

// Checks and decodes one received line. Returns true and fills `out` only if the line
// has the right format AND a correct checksum. A corrupted line is simply rejected.
bool BmeReader::parseLine(const std::string& line, BmeSample& out) {
    // The shortest valid line is longer than 8 characters and always starts with '$'.
    if (line.size() < 8 || line[0] != '$') return false;

    // Find the '*' that separates the data from the checksum. There must be room for
    // two hex digits after it (star + 3 <= size).
    const auto star = line.find('*');
    if (star == std::string::npos || star + 3 > line.size()) return false;

    // Calculate the checksum: XOR of every character between '$' and '*'
    // (the '$' at index 0 and the '*' itself are not included).
    uint8_t cs = 0;
    for (size_t i = 1; i < star; ++i) cs ^= static_cast<uint8_t>(line[i]);

    // Read the two hex digits after '*' and compare them with our own calculation.
    // A mismatch means bits were damaged on the way (noise, wrong baud rate, half a line).
    unsigned received = 0;
    if (std::sscanf(line.c_str() + star + 1, "%2x", &received) != 1 || received != cs) return false;

    // Extract the three numbers. sscanf returns how many fields it filled in; we require 3.
    // The trailing '*' in the format makes sure the pattern matches the whole data part.
    long t, p, h;
    if (std::sscanf(line.c_str(), "$BME,%ld,%ld,%ld*", &t, &p, &h) != 3) return false;
    out.tempCentiC = static_cast<int32_t>(t);
    out.pressurePa = static_cast<int32_t>(p);
    out.humCentiPct = static_cast<int32_t>(h);
    return true;
}

// Builds the JSON that is published, e.g.
// {"temperature_c":23.45,"pressure_hpa":1013.25,"humidity_pct":45.20}
std::string BmeReader::toJson(const BmeSample& s) {
    // Pressure in Pa equals hundredths of hPa (1013.25 hPa = 101325 Pa),
    // so fixed2 gives hPa with two decimals directly.
    return "{\"temperature_c\":" + fixed2(s.tempCentiC) +
           ",\"pressure_hpa\":" + fixed2(s.pressurePa) +
           ",\"humidity_pct\":" + fixed2(s.humCentiPct) + "}";
}

// The body of the background thread: keeps the serial port open, reads lines,
// validates them and publishes the result. Runs until stop() sets running_ to false.
void BmeReader::run() {
    std::string line;
    BmeSample sample;
    int badLines = 0;          // invalid lines in a row, used to limit log output
    bool warnedOpen = false;   // so the "cannot open" error is printed once, not every 2 s

    while (running_) {
        // Step 1: make sure the port is open. This also handles the STM32 being unplugged
        // and plugged in again later: the port is closed on a read error (see below)
        // and reopened here.
        if (!port_.isOpen()) {
            if (!port_.open(device_, baud_)) {
                if (!warnedOpen) {
                    std::cerr << "BME: kan " << device_ << " niet openen: " << port_.lastError() << "\n";
                    warnedOpen = true;
                }
                // Wait 2 seconds before retrying, in steps of 100 ms so stop() does not
                // have to wait for the full two seconds.
                for (int i = 0; i < 20 && running_; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            warnedOpen = false;
            std::cout << "BME: seriële poort " << device_ << " geopend\n";
        }

        // Step 2: wait up to 1 second for a complete line.
        if (!port_.readLine(line, 1000)) {
            // A plain timeout is normal (the STM32 sends one line per second, so a late line
            // is no problem). Any other error means the port is broken, e.g. the cable was
            // pulled out: close it so step 1 reopens it after a short pause.
            if (port_.lastError() != "Timeout bij lezen") {
                std::cerr << "BME: leesfout (" << port_.lastError() << "), poort wordt heropend\n";
                port_.close();
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            continue;
        }

        // Step 3: handle the line.
        // The STM32 itself reports that the sensor cannot be read (wiring/I2C problem).
        // The serial link works, so this is logged but nothing is published.
        if (line.rfind("$ERR", 0) == 0) {   // rfind(..., 0) = "starts with"
            std::cerr << "BME: STM32 meldt een sensorfout: " << line << "\n";
            continue;
        }

        // A valid measurement goes to MQTT. An invalid line (wrong checksum or format)
        // is dropped; only the 1st, 11th, 21st... bad line is logged so the console
        // does not fill up when the baud rate is wrong.
        if (parseLine(line, sample)) {
            badLines = 0;
            mqtt_.publish(topic_, toJson(sample));
        } else if (++badLines % 10 == 1) {
            std::cerr << "BME: ongeldige regel genegeerd: '" << line << "'\n";
        }
    }

    // Thread is stopping: release the serial port.
    port_.close();
}