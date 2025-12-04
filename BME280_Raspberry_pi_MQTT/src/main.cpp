#include "BME280.hpp"
#include "mqtt_client.hpp"
#include "../include/mqtt_config.h"


#include <iostream>
#include <unistd.h>
#include <sstream>
#include <ctime>


int main() {
BME280 sensor;
if (!sensor.begin()) {
std::cerr << "Failed to initialize BME280 sensor!" << std::endl;
return 1;
}


MQTTClient mqtt(MQTT_CLIENT_ID);
if (!mqtt.connect_async(MQTT_BROKER_HOST, MQTT_BROKER_PORT)) {
std::cerr << "Failed to start MQTT client" << std::endl;
// we continue: allow operation offline; publishes will fail until connected
}


while (true) {
float temp, press, hum;
sensor.readData(temp, press, hum);


// Build a JSON payload
std::ostringstream oss;
std::time_t t = std::time(nullptr);
oss << "{"
<< "\"timestamp\":" << t << ","
<< "\"temperature\":" << temp << ","
<< "\"pressure\":" << press << ","
<< "\"humidity\":" << hum
<< "}";


std::string payload = oss.str();


std::cout << "Publishing: " << payload << std::endl;
mqtt.publish(MQTT_TOPIC, payload, MQTT_QOS, MQTT_RETAIN);


sleep(PUBLISH_INTERVAL_SECONDS);
}


return 0;
}