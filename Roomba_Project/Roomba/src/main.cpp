/*
 * Main program: connects a Roomba and/or a BME280 (via an STM32) to MQTT on the Raspberry Pi.
 *
 * What the program does:
 *   1. Roomba (optional): reads the sensors every `pollMs` milliseconds and publishes them on
 *      <prefix>/sensors. Commands arrive on <prefix>/cmd/#.
 *   2. BME280 (optional): a separate thread (BmeReader) reads lines from the STM32 and
 *      publishes the measurements as JSON on <prefix>/bme280.
 *   3. MQTT: one shared connection to the broker, with an "online/offline" status.
 *
 * Example:
 *   ./roomba_mqtt --device none --bme-device /dev/ttyACM0 --host localhost
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "STM_Com.hpp"        
#include "CommandHandler.hpp" 
#include "MQTTClient.hpp"     
#include "Roomba.hpp"         
#include "SerialPort.hpp"     

namespace {

// Flag that becomes false when the user presses Ctrl+C. The main loop then stops cleanly.
// It is a std::atomic because the signal handler runs "in between" normal code.
std::atomic<bool> g_running{true};
void onSignal(int) { g_running = false; }

// All settings that can be given on the command line, with their default values.
struct Config {
    // --- Roomba ---
    std::string device = "/dev/ttyUSB0";  // Roomba; "none" = do not use a Roomba
    int baud = 115200;
    // --- MQTT ---
    std::string host = "localhost";
    int port = 1883;
    std::string user;
    std::string password;
    std::string prefix = "roomba";   // all topics start with this, e.g. roomba/status
    // --- Roomba behaviour ---
    std::string startMode = "safe";  // passive | safe | full
    int pollMs = 200;                // not faster than 15 ms (OI limit)

    // --- BME280 via STM32 ---
    std::string bmeDevice;           // empty = BME280 disabled
    int bmeBaud = 115200;
    std::string bmeTopic;            // empty = <prefix>/bme280
};

// Prints the available options (on --help or when an option is wrong).
void usage(const char* prog) {
    std::cout << "Gebruik: " << prog << " [opties]\n"
              << "  --device PAD       Roomba seriële poort (standaard /dev/ttyUSB0, 'none' = uit)\n"
              << "  --baud N           Roomba baudrate: 115200 of 19200 (standaard 115200)\n"
              << "  --host HOST        MQTT broker (standaard localhost)\n"
              << "  --port N           MQTT poort (standaard 1883)\n"
              << "  --user NAAM        MQTT gebruikersnaam\n"
              << "  --password PW      MQTT wachtwoord\n"
              << "  --prefix TOPIC     topic-prefix (standaard roomba)\n"
              << "  --mode MODE        passive | safe | full (standaard safe)\n"
              << "  --poll-ms N        Roomba sensor-interval in ms (standaard 200)\n"
              << "  --bme-device PAD   seriële poort van de STM32 met BME280 (bv. /dev/ttyACM0)\n"
              << "  --bme-baud N       baudrate van de STM32 (standaard 115200)\n"
              << "  --bme-topic TOPIC  MQTT-topic voor BME280 (standaard <prefix>/bme280)\n";
}

// Reads the command line into `c`. Returns false on --help or an unknown option.
bool parseArgs(int argc, char** argv, Config& c) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];

        // Helper: takes the next argument as the value of the current option.
        // Returns false if there is no value left (e.g. "--host" as the last word).
        auto next = [&](std::string& out) {
            if (i + 1 >= argc) return false;
            out = argv[++i];
            return true;
        };

        std::string v;
        if (a == "--help" || a == "-h") return false;
        else if (a == "--device" && next(v)) c.device = v;
        else if (a == "--baud" && next(v)) c.baud = std::atoi(v.c_str());
        else if (a == "--host" && next(v)) c.host = v;
        else if (a == "--port" && next(v)) c.port = std::atoi(v.c_str());
        else if (a == "--user" && next(v)) c.user = v;
        else if (a == "--password" && next(v)) c.password = v;
        else if (a == "--prefix" && next(v)) c.prefix = v;
        else if (a == "--mode" && next(v)) c.startMode = v;
        else if (a == "--poll-ms" && next(v)) c.pollMs = std::atoi(v.c_str());
        else if (a == "--bme-device" && next(v)) c.bmeDevice = v;
        else if (a == "--bme-baud" && next(v)) c.bmeBaud = std::atoi(v.c_str());
        else if (a == "--bme-topic" && next(v)) c.bmeTopic = v;
        else {
            std::cerr << "Onbekende of onvolledige optie: " << a << "\n";
            return false;
        }
    }

    // The Roomba refreshes its sensors every 15 ms; asking more often is pointless.
    if (c.pollMs < 15) c.pollMs = 15;
    // No BME topic given? Then use <prefix>/bme280.
    if (c.bmeTopic.empty()) c.bmeTopic = c.prefix + "/bme280";
    return true;
}
}  // namespace

int main(int argc, char** argv) {
    // ---------------------------------------------------------------------
    // 0. Read and check the options
    // ---------------------------------------------------------------------
    Config cfg;
    if (!parseArgs(argc, argv, cfg)) {
        usage(argv[0]);
        return 1;
    }
    // With "--device none" the program runs without a Roomba (BME280 only).
    const bool useRoomba = cfg.device != "none";
    if (!useRoomba && cfg.bmeDevice.empty()) {
        std::cerr << "Niets te doen: geef een Roomba (--device) en/of een BME280 (--bme-device) op.\n";
        return 1;
    }

    // Ctrl+C (SIGINT) or "kill" (SIGTERM) sets g_running to false, so that we can
    // shut down cleanly at the bottom (stop the wheels, put the Roomba to rest).
    struct sigaction sa{};
    sa.sa_handler = onSignal;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // ---------------------------------------------------------------------
    // 1. Serial connection to the Roomba (optional)
    // ---------------------------------------------------------------------
    SerialPort serial;
    Roomba roomba(serial);   // the object always exists, but is only used if useRoomba is true
    if (useRoomba) {
        if (!serial.open(cfg.device, cfg.baud)) {
            std::cerr << "Kan seriële poort niet openen: " << serial.lastError() << "\n";
            return 1;
        }
        roomba.start();  // start the OI -> Passive mode (the Roomba beeps once)
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        // Only in Safe or Full mode does the Roomba listen to drive and motor commands.
        if (cfg.startMode == "safe") roomba.safe();
        else if (cfg.startMode == "full") roomba.full();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // ---------------------------------------------------------------------
    // 2. Set up the MQTT connection
    // ---------------------------------------------------------------------
    const std::string statusTopic = cfg.prefix + "/status";    // "online" / "offline"
    const std::string sensorTopic = cfg.prefix + "/sensors";   // Roomba sensors as JSON

    MqttClient mqtt("roomba-pi");
    if (!cfg.user.empty()) mqtt.setCredentials(cfg.user, cfg.password);
    // Last will: the broker itself sets the status to "offline" if this program disappears.
    mqtt.setWill(statusTopic, "offline", true);

    // We only listen for commands when there is a Roomba. The CommandHandler lives in a
    // unique_ptr because it is only created when it is needed.
    std::unique_ptr<CommandHandler> handler;
    if (useRoomba) {
        handler = std::make_unique<CommandHandler>(roomba, cfg.prefix);
        // Every incoming message on <prefix>/cmd/# is passed to the handler.
        // Note: this runs on mosquitto's network thread, not on the main thread.
        mqtt.setOnMessage([&](const std::string& t, const std::string& p) { handler->handle(t, p); });
        mqtt.addSubscription(cfg.prefix + "/cmd/#");
    }
    // On every (re)connect, announce "online" again as a retained message.
    mqtt.setOnConnect([&] { mqtt.publish(statusTopic, "online", true); });

    // Connecting happens in the background; mosquitto retries by itself if the broker goes away.
    if (!mqtt.connect(cfg.host, cfg.port)) return 1;

    // ---------------------------------------------------------------------
    // 3. BME280 via STM32 (own thread)
    // ---------------------------------------------------------------------
    // The BmeReader reads lines from the STM32 and publishes to MQTT by itself,
    // so the main loop does not have to do anything for it.
    std::unique_ptr<BmeReader> bme;
    if (!cfg.bmeDevice.empty()) {
        bme = std::make_unique<BmeReader>(cfg.bmeDevice, cfg.bmeBaud, mqtt, cfg.bmeTopic);
        bme->start();
        std::cout << "BME280: lees " << cfg.bmeDevice << " -> '" << cfg.bmeTopic << "'\n";
    }
    if (useRoomba)
        std::cout << "Roomba: sensoren op '" << sensorTopic << "', commando's op '" << cfg.prefix << "/cmd/#'\n";

    // ---------------------------------------------------------------------
    // 4. Main loop: runs until Ctrl+C
    // ---------------------------------------------------------------------
    int failures = 0;      // number of failed reads in a row
    SensorData data;
    while (g_running) {
        if (useRoomba) {
            if (roomba.readSensors(data)) {
                failures = 0;
                mqtt.publish(sensorTopic, data.toJson());
            } else if (++failures % 10 == 1) {
                // Do not report every failure (it would flood the console): only the 1st, 11th, 21st...
                std::cerr << "Sensoren lezen mislukt (" << failures << "x): " << serial.lastError() << "\n";
            }
            // Watchdog: stops the wheels if no drive command has arrived for more than 1 s.
            handler->tick();
        }
        // Without a Roomba the loop only sleeps; the BME thread does the work.
        std::this_thread::sleep_for(std::chrono::milliseconds(cfg.pollMs));
    }

    // ---------------------------------------------------------------------
    // 5. Clean shutdown
    // ---------------------------------------------------------------------
    std::cout << "Afsluiten...\n";
    if (bme) bme->stop();            // stop the thread before MQTT is shut down
    if (useRoomba) {
        roomba.stopMotors();                          // stop the wheels
        mqtt.publish(statusTopic, "offline", true, 1);
        roomba.stop();  // OI off, protects the battery (Safe/Full mode do not charge)
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    mqtt.disconnect();
    return 0;
}