#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "include/CommandHandler.hpp"
#include "include/MQTTClient.hpp"
#include "include/Roomba.hpp"
#include "include/SerialPort.hpp"

namespace {
std::atomic<bool> g_running{true};
void onSignal(int) { g_running = false; }

struct Config {
    std::string device = "/dev/ttyUSB0";
    int baud = 115200;
    std::string host = "localhost";
    int port = 1883;
    std::string user;
    std::string password;
    std::string prefix = "roomba";
    std::string startMode = "safe";  // passive | safe | full
    int pollMs = 200;                // niet sneller dan 15 ms (OI-limiet)
};

void usage(const char* prog) {
    std::cout << "Gebruik: " << prog << " [opties]\n"
              << "  --device PAD     seriële poort (standaard /dev/ttyUSB0)\n"
              << "  --baud N         115200 of 19200 (standaard 115200)\n"
              << "  --host HOST      MQTT broker (standaard localhost)\n"
              << "  --port N         MQTT poort (standaard 1883)\n"
              << "  --user NAAM      MQTT gebruikersnaam\n"
              << "  --password PW    MQTT wachtwoord\n"
              << "  --prefix TOPIC   topic-prefix (standaard roomba)\n"
              << "  --mode MODE      passive | safe | full (standaard safe)\n"
              << "  --poll-ms N      sensor-interval in ms (standaard 200)\n";
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
        else {
            std::cerr << "Onbekende of onvolledige optie: " << a << "\n";
            return false;
        }
    }
    if (c.pollMs < 15) c.pollMs = 15;
    return true;
}
}  // namespace

int main(int argc, char** argv) {
    Config cfg;
    if (!parseArgs(argc, argv, cfg)) {
        usage(argv[0]);
        return 1;
    }

    struct sigaction sa{};
    sa.sa_handler = onSignal;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // 1. Seriële verbinding met de Roomba
    SerialPort serial;
    if (!serial.open(cfg.device, cfg.baud)) {
        std::cerr << "Kan seriële poort niet openen: " << serial.lastError() << "\n";
        return 1;
    }
    Roomba roomba(serial);
    roomba.start();  // OI starten -> Passive
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    if (cfg.startMode == "safe") roomba.safe();
    else if (cfg.startMode == "full") roomba.full();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 2. MQTT
    const std::string statusTopic = cfg.prefix + "/status";
    const std::string sensorTopic = cfg.prefix + "/sensors";

    MqttClient mqtt("roomba-pi");
    if (!cfg.user.empty()) mqtt.setCredentials(cfg.user, cfg.password);
    mqtt.setWill(statusTopic, "offline", true);

    CommandHandler handler(roomba, cfg.prefix);
    mqtt.setOnMessage([&](const std::string& t, const std::string& p) { handler.handle(t, p); });
    mqtt.setOnConnect([&] { mqtt.publish(statusTopic, "online", true); });
    mqtt.addSubscription(cfg.prefix + "/cmd/#");

    if (!mqtt.connect(cfg.host, cfg.port)) return 1;

    // 3. Hoofdlus: sensoren lezen en publiceren
    std::cout << "Gestart. Sensoren op '" << sensorTopic << "', commando's op '" << cfg.prefix << "/cmd/#'\n";
    int failures = 0;
    SensorData data;
    while (g_running) {
        if (roomba.readSensors(data)) {
            failures = 0;
            mqtt.publish(sensorTopic, data.toJson());
        } else if (++failures % 10 == 1) {
            std::cerr << "Sensoren lezen mislukt (" << failures << "x): " << serial.lastError() << "\n";
        }
        handler.tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(cfg.pollMs));
    }

    // 4. Netjes afsluiten: wielen stoppen en OI uitzetten (beschermt de accu)
    std::cout << "Afsluiten...\n";
    roomba.stopMotors();
    mqtt.publish(statusTopic, "offline", true, 1);
    roomba.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    mqtt.disconnect();
    return 0;
}