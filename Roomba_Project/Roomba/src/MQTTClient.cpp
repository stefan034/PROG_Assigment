#include "MQTTClient.hpp"

#include <iostream>
#include <mosquitto.h>
#include <stdexcept>

MqttClient::MqttClient(const std::string& clientId) {
    mosquitto_lib_init();
    mosq_ = mosquitto_new(clientId.c_str(), true, this);
    if (!mosq_) throw std::runtime_error("mosquitto_new mislukt");
    mosquitto_connect_callback_set(mosq_, &MqttClient::onConnectStatic);
    mosquitto_message_callback_set(mosq_, &MqttClient::onMessageStatic);
    mosquitto_reconnect_delay_set(mosq_, 2, 30, true);
}

MqttClient::~MqttClient() {
    disconnect();
    if (mosq_) mosquitto_destroy(mosq_);
    mosquitto_lib_cleanup();
}

void MqttClient::setWill(const std::string& topic, const std::string& payload, bool retain) {
    mosquitto_will_set(mosq_, topic.c_str(), static_cast<int>(payload.size()), payload.data(), 1, retain);
}

void MqttClient::setCredentials(const std::string& user, const std::string& password) {
    mosquitto_username_pw_set(mosq_, user.c_str(), password.empty() ? nullptr : password.c_str());
}

bool MqttClient::connect(const std::string& host, int port) {
    int rc = mosquitto_connect_async(mosq_, host.c_str(), port, 30);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "MQTT connect fout: " << mosquitto_strerror(rc) << "\n";
        return false;
    }
    rc = mosquitto_loop_start(mosq_);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "MQTT loop_start fout: " << mosquitto_strerror(rc) << "\n";
        return false;
    }
    loopStarted_ = true;
    return true;
}

void MqttClient::disconnect() {
    if (!mosq_ || !loopStarted_) return;
    mosquitto_disconnect(mosq_);
    mosquitto_loop_stop(mosq_, false);
    loopStarted_ = false;
}

bool MqttClient::publish(const std::string& topic, const std::string& payload, bool retain, int qos) {
    int rc = mosquitto_publish(mosq_, nullptr, topic.c_str(), static_cast<int>(payload.size()),
                               payload.data(), qos, retain);
    return rc == MOSQ_ERR_SUCCESS;
}

void MqttClient::onConnectStatic(mosquitto*, void* self, int rc) {
    auto* c = static_cast<MqttClient*>(self);
    if (rc != 0) {
        std::cerr << "MQTT verbinding geweigerd, rc=" << rc << "\n";
        return;
    }
    std::cout << "MQTT verbonden\n";
    for (const auto& t : c->subscriptions_) mosquitto_subscribe(c->mosq_, nullptr, t.c_str(), 1);
    if (c->onConnect_) c->onConnect_();
}

void MqttClient::onMessageStatic(mosquitto*, void* self, const mosquitto_message* msg) {
    auto* c = static_cast<MqttClient*>(self);
    if (!c->onMessage_ || !msg->topic) return;
    std::string payload(static_cast<const char*>(msg->payload), static_cast<size_t>(msg->payloadlen));
    c->onMessage_(msg->topic, payload);
}