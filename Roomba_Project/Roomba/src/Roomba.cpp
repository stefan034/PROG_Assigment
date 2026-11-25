/*
 * Roomba: implements the subset of the iRobot Create 2 / Roomba 600 Open Interface (OI)
 * that this project needs. Every OI command is a one-byte opcode, optionally followed by
 * data bytes (see the OI specification). Multi-byte values are sent HIGH BYTE FIRST.
 *
 * Mode requirements (from the spec):
 *   - Sensor and cleaning commands work in Passive, Safe and Full mode.
 *   - Drive and motor commands only work in Safe or Full mode.
 *
 * Thread safety: every public method takes `mutex_` before touching the serial port,
 * because commands arrive from the MQTT thread while the main loop reads sensors.
 * Without it, a command could be written in the middle of a sensor query.
 */

#include "Roomba.hpp"

#include <algorithm>
#include <vector>

namespace {
// Helpers for converting between 16-bit values and the two bytes the OI uses.
// The cast to uint16_t first makes the shift well-defined for negative numbers,
// so negative values come out as two's complement (e.g. -200 -> 0xFF38 -> [255][56]).

// High byte (bits 15..8) of a 16-bit value.
inline uint8_t hi(int16_t v) { return static_cast<uint8_t>((static_cast<uint16_t>(v) >> 8) & 0xFF); }
// Low byte (bits 7..0) of a 16-bit value.
inline uint8_t lo(int16_t v) { return static_cast<uint8_t>(static_cast<uint16_t>(v) & 0xFF); }
// Combines two received bytes (high byte first) into an unsigned 16-bit value.
inline uint16_t u16(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }
// Same, but interpreted as signed (two's complement), e.g. for current and encoder counts.
inline int16_t s16(const uint8_t* p) { return static_cast<int16_t>(u16(p)); }
}  // namespace

// Writes a complete command (opcode + data bytes) to the serial port in one go.
// Private helper: the CALLER must already hold `mutex_`, so it does not lock itself.
bool Roomba::send(std::initializer_list<uint8_t> bytes) {
    std::vector<uint8_t> buf(bytes);
    return port_.write(buf.data(), buf.size());
}

// ---------------------------------------------------------------------------
// Mode commands
// ---------------------------------------------------------------------------

// Opcode 128: starts the Open Interface. Must be sent before any other command.
// The Roomba switches to Passive mode and beeps once.
bool Roomba::start()   { std::lock_guard<std::mutex> l(mutex_); return send({128}); }
// Passive mode is entered with the same Start command, hence the alias.
bool Roomba::passive() { return start(); }
// Opcode 131: Safe mode. Full control, but the Roomba still stops itself on a cliff,
// wheel drop or when the charger is connected.
bool Roomba::safe()    { std::lock_guard<std::mutex> l(mutex_); return send({131}); }
// Opcode 132: Full mode. All safety features are off, so the robot can drive off a ledge.
bool Roomba::full()    { std::lock_guard<std::mutex> l(mutex_); return send({132}); }
// Opcode 173: stops the OI. Use this when finished so the battery is not drained in Safe/Full mode.
bool Roomba::stop()    { std::lock_guard<std::mutex> l(mutex_); return send({173}); }
// Opcode 7: resets the robot as if the battery had been removed. Afterwards Start (128) is needed again.
bool Roomba::reset()   { std::lock_guard<std::mutex> l(mutex_); return send({7}); }

// ---------------------------------------------------------------------------
// Cleaning commands (the robot returns to Passive mode after these)
// ---------------------------------------------------------------------------

// Opcode 135: default cleaning mode, same as pressing the Clean button (pauses if already running).
bool Roomba::clean()    { std::lock_guard<std::mutex> l(mutex_); return send({135}); }
// Opcode 136: Max cleaning mode, cleans until the battery is empty.
bool Roomba::max()      { std::lock_guard<std::mutex> l(mutex_); return send({136}); }
// Opcode 134: spot cleaning, cleans a small area in a spiral.
bool Roomba::spot()     { std::lock_guard<std::mutex> l(mutex_); return send({134}); }
// Opcode 143: drive to the dock the next time the docking beams are found.
bool Roomba::seekDock() { std::lock_guard<std::mutex> l(mutex_); return send({143}); }
// Opcode 133: powers the robot down.
bool Roomba::power()    { std::lock_guard<std::mutex> l(mutex_); return send({133}); }

// ---------------------------------------------------------------------------
// Actuator commands (Safe or Full mode only)
// ---------------------------------------------------------------------------

// Opcode 137: drive with an average wheel velocity and a turning radius.
//   velocity: -500..500 mm/s (negative = backward)
//   radius:   -2000..2000 mm (positive = turn left, negative = turn right)
//   special radius values: 32767 = straight, 1 = spin counter-clockwise, -1 = spin clockwise
// Input is clamped to the legal range first. The special value 32767 is exempt from clamping,
// otherwise "straight" would be turned into a 2000 mm arc.
// Each 16-bit value is sent as high byte, then low byte.
bool Roomba::drive(int16_t velocityMmS, int16_t radiusMm) {
    velocityMmS = std::clamp<int16_t>(velocityMmS, -500, 500);
    if (radiusMm != kRadiusStraight) radiusMm = std::clamp<int16_t>(radiusMm, -2000, 2000);
    std::lock_guard<std::mutex> l(mutex_);
    return send({137, hi(velocityMmS), lo(velocityMmS), hi(radiusMm), lo(radiusMm)});
}

// Opcode 145: control the right and left wheel independently, each -500..500 mm/s.
// Note the order in the command: RIGHT wheel first, then LEFT.
// Opposite speeds make the robot spin in place.
bool Roomba::driveDirect(int16_t rightMmS, int16_t leftMmS) {
    rightMmS = std::clamp<int16_t>(rightMmS, -500, 500);
    leftMmS = std::clamp<int16_t>(leftMmS, -500, 500);
    std::lock_guard<std::mutex> l(mutex_);
    return send({145, hi(rightMmS), lo(rightMmS), hi(leftMmS), lo(leftMmS)});
}

// Stops both wheels by commanding 0 mm/s. It calls driveDirect(), which takes the
// mutex itself, so no lock is held here (locking twice would deadlock).
bool Roomba::stopMotors() { return driveDirect(0, 0); }

// Opcode 144: PWM duty cycle for the main brush, side brush and vacuum.
//   brushes: -127..127 (negative = reverse direction)
//   vacuum:  0..127 (only runs forward)
// A value of 127 is the maximum; the spec describes the duty cycle as a fraction of 128.
// int8_t already caps the brushes at +127, so only the lower bound (-128 -> -127) needs a clamp.
bool Roomba::setMotorsPwm(int8_t mainBrush, int8_t sideBrush, uint8_t vacuum) {
    vacuum = std::min<uint8_t>(vacuum, 127);
    mainBrush = std::max<int8_t>(mainBrush, -127);
    sideBrush = std::max<int8_t>(sideBrush, -127);
    std::lock_guard<std::mutex> l(mutex_);
    return send({144, static_cast<uint8_t>(mainBrush), static_cast<uint8_t>(sideBrush), vacuum});
}

// Opcode 139: controls the LEDs.
//   ledBits:        bit 0 debris, bit 1 spot, bit 2 dock/home, bit 3 check robot
//   powerColor:     0 = green .. 255 = red (values in between are orange/yellow)
//   powerIntensity: 0 = off .. 255 = full brightness
bool Roomba::setLeds(uint8_t ledBits, uint8_t powerColor, uint8_t powerIntensity) {
    std::lock_guard<std::mutex> l(mutex_);
    return send({139, ledBits, powerColor, powerIntensity});
}

// ---------------------------------------------------------------------------
// Sensors
// ---------------------------------------------------------------------------

// Reads several sensors in ONE request using the Query List command (opcode 149):
//   [149] [number of packets] [packet id 1] [packet id 2] ...
// The Roomba answers with the data of each packet, in the order requested, without any
// header or checksum. So we must know the size of every packet to decode the reply.
// Returns false on a write error or if the full reply does not arrive in time.
bool Roomba::readSensors(SensorData& out) {
    // Packet IDs and their size in bytes (see the sensor packet section of the OI spec):
    //   7:1 bumps and wheel drops   21:1 charging state   22:2 battery voltage (mV)
    //   23:2 battery current (mA)   24:1 temperature (C)  25:2 charge (mAh)
    //   26:2 capacity (mAh)         35:1 OI mode          45:1 light bumper bits
    //   43:2 left encoder           44:2 right encoder    13:1 virtual wall
    //   => 18 bytes in total
    static const uint8_t kPackets[] = {7, 21, 22, 23, 24, 25, 26, 35, 45, 43, 44, 13};
    static constexpr size_t kTotal = 1 + 1 + 2 + 2 + 1 + 2 + 2 + 1 + 1 + 2 + 2 + 1;

    std::lock_guard<std::mutex> l(mutex_);

    // Throw away stale bytes (e.g. a previous half-received reply), otherwise the new
    // answer would be misaligned and every value decoded below would be wrong.
    port_.flushInput();

    // Build the request: opcode, number of packets, then the packet IDs themselves.
    std::vector<uint8_t> cmd{149, static_cast<uint8_t>(sizeof(kPackets))};
    cmd.insert(cmd.end(), std::begin(kPackets), std::end(kPackets));
    if (!port_.write(cmd.data(), cmd.size())) return false;

    // Wait for exactly kTotal bytes, with a 100 ms timeout. At 115200 baud 18 bytes take
    // about 1.6 ms, so a timeout means the robot is asleep, in the wrong mode, or not connected.
    uint8_t b[kTotal];
    if (!port_.readExact(b, kTotal, 100)) return false;

    // Decode the reply in the same order as requested. `i` is the current read position;
    // 2-byte values (high byte first) advance it by two.
    size_t i = 0;
    out.bumpsAndDrops = b[i++];                      // packet 7: bit flags
    out.chargingState = b[i++];                      // packet 21: 0..5
    out.voltageMv = u16(&b[i]);      i += 2;         // packet 22: unsigned
    out.currentMa = s16(&b[i]);      i += 2;         // packet 23: signed (negative = discharging)
    out.temperatureC = static_cast<int8_t>(b[i++]);  // packet 24: signed byte
    out.chargeMah = u16(&b[i]);      i += 2;         // packet 25
    out.capacityMah = u16(&b[i]);    i += 2;         // packet 26
    // Packet 35: 0 off, 1 passive, 2 safe, 3 full. Anything else is treated as unknown.
    out.mode = b[i] <= 3 ? static_cast<OiMode>(b[i]) : OiMode::Unknown;
    i++;
    out.lightBumper = b[i++];                        // packet 45: one bit per light bumper
    out.leftEncoder = s16(&b[i]);    i += 2;         // packet 43: wraps around at +/-32768
    out.rightEncoder = s16(&b[i]);   i += 2;         // packet 44
    out.virtualWall = b[i++] != 0;                   // packet 13: 0 or 1
    return true;
}