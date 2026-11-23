#include "STM_Com.hpp"

#include <chrono>
#include <cstdio>
#include <iostream>

namespace {
// v is in honderdsten: 2345 -> "23.45", -305 -> "-3.05"
std::string fixed2(int32_t v) {
    char buf[32];
    const char* sign = v < 0 ? "-" : "";
    const int64_t a = v < 0 ? -static_cast<int64_t>(v) : v;
    std::snprintf(buf, sizeof(buf), "%s%d.%02d", sign, static_cast<int>(a / 100), static_cast<int>(a % 100));
    return buf;
}
}  // namespace

BmeReader::BmeReader(std::string device, int baud, MqttClient& mqtt, std::string topic)
    : device_(std::move(device)), baud_(baud), mqtt_(mqtt), topic_(std::move(topic)) {}

BmeReader::~BmeReader() { stop(); }

void BmeReader::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&BmeReader::run, this);
}

void BmeReader::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

bool BmeReader::parseLine(const std::string& line, BmeSample& out) {
    if (line.size() < 8 || line[0] != '$') return false;
    const auto star = line.find('*');
    if (star == std::string::npos || star + 3 > line.size()) return false;

    uint8_t cs = 0;
    for (size_t i = 1; i < star; ++i) cs ^= static_cast<uint8_t>(line[i]);
    unsigned received = 0;
    if (std::sscanf(line.c_str() + star + 1, "%2x", &received) != 1 || received != cs) return false;

    long t, p, h;
    if (std::sscanf(line.c_str(), "$BME,%ld,%ld,%ld*", &t, &p, &h) != 3) return false;
    out.tempCentiC = static_cast<int32_t>(t);
    out.pressurePa = static_cast<int32_t>(p);
    out.humCentiPct = static_cast<int32_t>(h);
    return true;
}

std::string BmeReader::toJson(const BmeSample& s) {
    // Druk in Pa == honderdsten hPa, dus fixed2 geeft direct hPa met 2 decimalen.
    return "{\"temperature_c\":" + fixed2(s.tempCentiC) +
           ",\"pressure_hpa\":" + fixed2(s.pressurePa) +
           ",\"humidity_pct\":" + fixed2(s.humCentiPct) + "}";
}

void BmeReader::run() {
    std::string line;
    BmeSample sample;
    int badLines = 0;
    bool warnedOpen = false;

    while (running_) {
        if (!port_.isOpen()) {
            if (!port_.open(device_, baud_)) {
                if (!warnedOpen) {
                    std::cerr << "BME: kan " << device_ << " niet openen: " << port_.lastError() << "\n";
                    warnedOpen = true;
                }
                for (int i = 0; i < 20 && running_; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            warnedOpen = false;
            std::cout << "BME: seriële poort " << device_ << " geopend\n";
        }

        if (!port_.readLine(line, 1000)) {
            if (port_.lastError() != "Timeout bij lezen") {
                std::cerr << "BME: leesfout (" << port_.lastError() << "), poort wordt heropend\n";
                port_.close();
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            continue;
        }

        if (line.rfind("$ERR", 0) == 0) {
            std::cerr << "BME: STM32 meldt een sensorfout: " << line << "\n";
            continue;
        }
        if (parseLine(line, sample)) {
            badLines = 0;
            mqtt_.publish(topic_, toJson(sample));
        } else if (++badLines % 10 == 1) {
            std::cerr << "BME: ongeldige regel genegeerd: '" << line << "'\n";
        }
    }
    port_.close();
}