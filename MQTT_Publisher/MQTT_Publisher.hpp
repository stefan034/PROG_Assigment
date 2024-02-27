#ifndef MQTT_PUBLISHER_HPP
#define MQTT_PUBLISHER_HPP

#include <string>
#include <async_client.h>

class MQTTPublisher {
public:
    MQTTPublisher(const std::string& serverAddress, const std::string& clientId);
    ~MQTTPublisher();

    void connect();
    void disconnect();
    void publish(const std::string& topic, const std::string& message);

private:
    std::string serverAddress_;
    std::string clientId_;
    mqtt::async_client client_;
};

#endif // MQTT_PUBLISHER_HPP
