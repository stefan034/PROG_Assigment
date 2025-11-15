#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H


// Broker settings
#define MQTT_BROKER_HOST "test.mosquitto.org"
#define MQTT_BROKER_PORT 1883


// Topic where sensor data will be published
#define MQTT_TOPIC "sensors/bme280/raspi"


// Client id (optional) — keep unique if you run multiple clients
#define MQTT_CLIENT_ID "bme280_raspi_01"


// Publish interval in seconds
#define PUBLISH_INTERVAL_SECONDS 2


// QoS (0,1,2)
#define MQTT_QOS 0


// Set to 1 to retain messages on the broker
#define MQTT_RETAIN 0


#endif // MQTT_CONFIG_H