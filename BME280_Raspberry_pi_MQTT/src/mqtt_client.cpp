#include "mqtt_client.hpp"
#include <iostream>
#include <cstring>


MQTTClient::MQTTClient(const std::string &clientId) : mosq(nullptr), id(clientId) {
mosquitto_lib_init();
mosq = mosquitto_new(id.c_str(), true, nullptr);
if (!mosq) {
std::cerr << "Failed to create mosquitto instance\n";
} else {
mosquitto_connect_callback_set(mosq, MQTTClient::on_connect);
mosquitto_disconnect_callback_set(mosq, MQTTClient::on_disconnect);
}
}


MQTTClient::~MQTTClient() {
if (mosq) {
mosquitto_disconnect(mosq);
mosquitto_loop_stop(mosq, true);
mosquitto_destroy(mosq);
}
mosquitto_lib_cleanup();
}


bool MQTTClient::connect_async(const std::string &host, int port, int keepalive) {
if (!mosq) return false;
int rc = mosquitto_connect_async(mosq, host.c_str(), port, keepalive);
if (rc != MOSQ_ERR_SUCCESS) {
std::cerr << "mosquitto_connect_async failed: " << mosquitto_strerror(rc) << std::endl;
return false;
}
rc = mosquitto_loop_start(mosq);
if (rc != MOSQ_ERR_SUCCESS) {
std::cerr << "mosquitto_loop_start failed: " << mosquitto_strerror(rc) << std::endl;
return false;
}
return true;
}


void MQTTClient::disconnect() {
if (!mosq) return;
mosquitto_disconnect(mosq);
}


bool MQTTClient::publish(const std::string &topic, const std::string &payload, int qos, bool retain) {
if (!mosq) return false;
int mid;
int rc = mosquitto_publish(mosq, &mid, topic.c_str(), (int)payload.size(), payload.c_str(), qos, retain);
if (rc != MOSQ_ERR_SUCCESS) {
std::cerr << "mosquitto_publish failed: " << mosquitto_strerror(rc) << std::endl;
return false;
}
return true;
}


void MQTTClient::on_connect(struct mosquitto *mosq, void *obj, int rc) {
if (rc == 0) std::cout << "MQTT connected\n";
else std::cerr << "MQTT connect failed, rc=" << rc << "\n";
}


void MQTTClient::on_disconnect(struct mosquitto *mosq, void *obj, int rc) {
std::cout << "MQTT disconnected (rc=" << rc << ")\n";
}