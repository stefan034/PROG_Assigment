#include "Roomba.hpp"

#include <algorithm>
#include <vector>

namespace {
inline uint8_t hi(int16_t v) { return static_cast<uint8_t>((static_cast<uint16_t>(v) >> 8) & 0xFF); }
inline uint8_t lo(int16_t v) { return static_cast<uint8_t>(static_cast<uint16_t>(v) & 0xFF); }
inline uint16_t u16(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }
inline int16_t s16(const uint8_t* p) { return static_cast<int16_t>(u16(p)); }
}  // namespace

bool Roomba::send(std::initializer_list<uint8_t> bytes) {
    std::vector<uint8_t> buf(bytes);
    return port_.write(buf.data(), buf.size());
}

bool Roomba::start()   { std::lock_guard<std::mutex> l(mutex_); return send({128}); }
bool Roomba::passive() { return start(); }
bool Roomba::safe()    { std::lock_guard<std::mutex> l(mutex_); return send({131}); }
bool Roomba::full()    { std::lock_guard<std::mutex> l(mutex_); return send({132}); }
bool Roomba::stop()    { std::lock_guard<std::mutex> l(mutex_); return send({173}); }
bool Roomba::reset()   { std::lock_guard<std::mutex> l(mutex_); return send({7}); }

bool Roomba::clean()    { std::lock_guard<std::mutex> l(mutex_); return send({135}); }
bool Roomba::max()      { std::lock_guard<std::mutex> l(mutex_); return send({136}); }
bool Roomba::spot()     { std::lock_guard<std::mutex> l(mutex_); return send({134}); }
bool Roomba::seekDock() { std::lock_guard<std::mutex> l(mutex_); return send({143}); }
bool Roomba::power()    { std::lock_guard<std::mutex> l(mutex_); return send({133}); }

bool Roomba::drive(int16_t velocityMmS, int16_t radiusMm) {
    velocityMmS = std::clamp<int16_t>(velocityMmS, -500, 500);
    if (radiusMm != kRadiusStraight) radiusMm = std::clamp<int16_t>(radiusMm, -2000, 2000);
    std::lock_guard<std::mutex> l(mutex_);
    return send({137, hi(velocityMmS), lo(velocityMmS), hi(radiusMm), lo(radiusMm)});
}

bool Roomba::driveDirect(int16_t rightMmS, int16_t leftMmS) {
    rightMmS = std::clamp<int16_t>(rightMmS, -500, 500);
    leftMmS = std::clamp<int16_t>(leftMmS, -500, 500);
    std::lock_guard<std::mutex> l(mutex_);
    return send({145, hi(rightMmS), lo(rightMmS), hi(leftMmS), lo(leftMmS)});
}

bool Roomba::stopMotors() { return driveDirect(0, 0); }

bool Roomba::setMotorsPwm(int8_t mainBrush, int8_t sideBrush, uint8_t vacuum) {
    vacuum = std::min<uint8_t>(vacuum, 127);
    mainBrush = std::max<int8_t>(mainBrush, -127);
    sideBrush = std::max<int8_t>(sideBrush, -127);
    std::lock_guard<std::mutex> l(mutex_);
    return send({144, static_cast<uint8_t>(mainBrush), static_cast<uint8_t>(sideBrush), vacuum});
}

bool Roomba::setLeds(uint8_t ledBits, uint8_t powerColor, uint8_t powerIntensity) {
    std::lock_guard<std::mutex> l(mutex_);
    return send({139, ledBits, powerColor, powerIntensity});
}

bool Roomba::readSensors(SensorData& out) {
    // Volgorde en grootte (bytes) van de opgevraagde packets.
    // 7:1  21:1  22:2  23:2  24:1  25:2  26:2  35:1  45:1  43:2  44:2  13:1  => 18 bytes
    static const uint8_t kPackets[] = {7, 21, 22, 23, 24, 25, 26, 35, 45, 43, 44, 13};
    static constexpr size_t kTotal = 1 + 1 + 2 + 2 + 1 + 2 + 2 + 1 + 1 + 2 + 2 + 1;

    std::lock_guard<std::mutex> l(mutex_);
    port_.flushInput();
    std::vector<uint8_t> cmd{149, static_cast<uint8_t>(sizeof(kPackets))};
    cmd.insert(cmd.end(), std::begin(kPackets), std::end(kPackets));
    if (!port_.write(cmd.data(), cmd.size())) return false;

    uint8_t b[kTotal];
    if (!port_.readExact(b, kTotal, 100)) return false;

    size_t i = 0;
    out.bumpsAndDrops = b[i++];
    out.chargingState = b[i++];
    out.voltageMv = u16(&b[i]);      i += 2;
    out.currentMa = s16(&b[i]);      i += 2;
    out.temperatureC = static_cast<int8_t>(b[i++]);
    out.chargeMah = u16(&b[i]);      i += 2;
    out.capacityMah = u16(&b[i]);    i += 2;
    out.mode = b[i] <= 3 ? static_cast<OiMode>(b[i]) : OiMode::Unknown;
    i++;
    out.lightBumper = b[i++];
    out.leftEncoder = s16(&b[i]);    i += 2;
    out.rightEncoder = s16(&b[i]);   i += 2;
    out.virtualWall = b[i++] != 0;
    return true;
}