#include "MQTT_Publisher.hpp"

MQTTPublisher::MQTTPublisher(const std::string& serverAddress, const std::string& clientId)
    : serverAddress_(serverAddress), clientId_(clientId), client_(serverAddress, clientId) {}

MQTTPublisher::~MQTTPublisher() {
    disconnect();
}

void MQTTPublisher::connect() {
    mqtt::connect_options conn_opts;
    conn_opts.set_keep_alive_interval(20);
    conn_opts.set_clean_session(true);

    client_.connect(conn_opts)->wait();
}

void MQTTPublisher::disconnect() {
    client_.disconnect()->wait();
}

void MQTTPublisher::publish(const std::string& topic, const std::string& message) {
    mqtt::message_ptr pubmsg = mqtt::make_message(topic, message);
    client_.publish(pubmsg)->wait();
}
