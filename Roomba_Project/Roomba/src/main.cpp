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
std::atomic<bool> g_running{true};
void onSignal(int) { g_running = false; }

struct Config {
    std::string device = "/dev/ttyUSB0";  // Roomba; "none" = geen Roomba gebruiken
    int baud = 115200;
    std::string host = "localhost";
    int port = 1883;
    std::string user;
    std::string password;
    std::string prefix = "roomba";
    std::string startMode = "safe";  // passive | safe | full
    int pollMs = 200;                // niet sneller dan 15 ms (OI-limiet)

    std::string bmeDevice;           // leeg = BME280 uitgeschakeld
    int bmeBaud = 115200;
    std::string bmeTopic;            // leeg = <prefix>/bme280
};

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

bool parseArgs(int argc, char** argv, Config& c) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
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
    if (c.pollMs < 15) c.pollMs = 15;
    if (c.bmeTopic.empty()) c.bmeTopic = c.prefix + "/bme280";
    return true;
}
}  // namespace

int main(int argc, char** argv) {
    Config cfg;
    if (!parseArgs(argc, argv, cfg)) {
        usage(argv[0]);
        return 1;
    }
    const bool useRoomba = cfg.device != "none";
    if (!useRoomba && cfg.bmeDevice.empty()) {
        std::cerr << "Niets te doen: geef een Roomba (--device) en/of een BME280 (--bme-device) op.\n";
        return 1;
    }

    struct sigaction sa{};
    sa.sa_handler = onSignal;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // 1. Seriële verbinding met de Roomba (optioneel)
    SerialPort serial;
    Roomba roomba(serial);
    if (useRoomba) {
        if (!serial.open(cfg.device, cfg.baud)) {
            std::cerr << "Kan seriële poort niet openen: " << serial.lastError() << "\n";
            return 1;
        }
        roomba.start();  // OI starten -> Passive
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (cfg.startMode == "safe") roomba.safe();
        else if (cfg.startMode == "full") roomba.full();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 2. MQTT
    const std::string statusTopic = cfg.prefix + "/status";
    const std::string sensorTopic = cfg.prefix + "/sensors";

    MqttClient mqtt("roomba-pi");
    if (!cfg.user.empty()) mqtt.setCredentials(cfg.user, cfg.password);
    mqtt.setWill(statusTopic, "offline", true);

    std::unique_ptr<CommandHandler> handler;
    if (useRoomba) {
        handler = std::make_unique<CommandHandler>(roomba, cfg.prefix);
        mqtt.setOnMessage([&](const std::string& t, const std::string& p) { handler->handle(t, p); });
        mqtt.addSubscription(cfg.prefix + "/cmd/#");
    }
    mqtt.setOnConnect([&] { mqtt.publish(statusTopic, "online", true); });

    if (!mqtt.connect(cfg.host, cfg.port)) return 1;

    // 3. BME280 via STM32 (eigen thread)
    std::unique_ptr<BmeReader> bme;
    if (!cfg.bmeDevice.empty()) {
        bme = std::make_unique<BmeReader>(cfg.bmeDevice, cfg.bmeBaud, mqtt, cfg.bmeTopic);
        bme->start();
        std::cout << "BME280: lees " << cfg.bmeDevice << " -> '" << cfg.bmeTopic << "'\n";
    }
    if (useRoomba)
        std::cout << "Roomba: sensoren op '" << sensorTopic << "', commando's op '" << cfg.prefix << "/cmd/#'\n";

    // 4. Hoofdlus
    int failures = 0;
    SensorData data;
    while (g_running) {
        if (useRoomba) {
            if (roomba.readSensors(data)) {
                failures = 0;
                mqtt.publish(sensorTopic, data.toJson());
            } else if (++failures % 10 == 1) {
                std::cerr << "Sensoren lezen mislukt (" << failures << "x): " << serial.lastError() << "\n";
            }
            handler->tick();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(cfg.pollMs));
    }

    // 5. Netjes afsluiten
    std::cout << "Afsluiten...\n";
    if (bme) bme->stop();
    if (useRoomba) {
        roomba.stopMotors();
        mqtt.publish(statusTopic, "offline", true, 1);
        roomba.stop();  // OI uit, beschermt de accu
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    mqtt.disconnect();
    return 0;
}