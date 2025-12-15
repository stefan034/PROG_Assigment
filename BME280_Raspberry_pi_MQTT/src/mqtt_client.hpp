#ifndef MQTT_CLIENT_HPP
#define MQTT_CLIENT_HPP


#include <string>
#include <mosquitto.h>


class MQTTClient {
public:
MQTTClient(const std::string &clientId);
~MQTTClient();


bool connect_async(const std::string &host, int port, int keepalive = 60);
void disconnect();
bool publish(const std::string &topic, const std::string &payload, int qos = 0, bool retain = false);


private:
struct mosquitto *mosq;
std::string id;
static void on_connect(struct mosquitto *mosq, void *obj, int rc);
static void on_disconnect(struct mosquitto *mosq, void *obj, int rc);
};


#endif // MQTT_CLIENT_HPP