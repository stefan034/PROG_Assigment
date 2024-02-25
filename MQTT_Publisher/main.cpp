#include <iostream>
#include "MQTT_Publisher.hpp"

int main() {
    std::string serverAddress = "tcp://192.168.137.129:1883";
    std::string clientId = "publisher";
    std::string topic = "test";

    MQTTPublisher publisher(serverAddress, clientId);

    try {
        std::cout << "Connecting to the MQTT server..." << std::endl;
        publisher.connect();

        std::string message;
        std::cout << "Enter message to publish: ";
        std::getline(std::cin, message);

        publisher.publish(topic, message);

        std::cout << "Message published." << std::endl;

        publisher.disconnect();
    } catch (const mqtt::exception& exc) {
        std::cerr << "Error: " << exc.what() << std::endl;
        return 1;
    }

    return 0;
}
