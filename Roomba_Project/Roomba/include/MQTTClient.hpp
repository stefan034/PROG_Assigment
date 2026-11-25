#pragma once
#include <functional>
#include <string>
#include <vector>

struct mosquitto;

// Wrapper around libmosquitto with automatic reconnect and resubscribe on topics.
class MqttClient {
public:
    using MessageHandler = std::function<void(const std::string& topic, const std::string& payload)>;
    using ConnectHandler = std::function<void()>;

    explicit MqttClient(const std::string& clientId);
    ~MqttClient();
    MqttClient(const MqttClient&) = delete;
    MqttClient& operator=(const MqttClient&) = delete;

    // Must be called before connect().
    /// @brief Sets the "last will" message that the broker sends if this client disconnects unexpectedly.
    /// @param topic The topic to publish the will message to.
    /// @param payload The payload of the will message.
    /// @param retain Whether the will message should be retained.
    /// @return True if the will message was set successfully, false otherwise.
    void setWill(const std::string& topic, const std::string& payload, bool retain);

    /// @brief Sets the credentials for the MQTT connection.
    /// @param user The username for the MQTT connection.
    /// @param password The password for the MQTT connection.
    /// @return True if the credentials were set successfully, false otherwise.
    void setCredentials(const std::string& user, const std::string& password);

    /// @brief Sets the callback function that is called when a message is received on a subscribed topic.
    /// @param handler The callback function.
    /// @return True if the callback was set successfully, false otherwise.
    void setOnMessage(MessageHandler handler) { onMessage_ = std::move(handler); }

    /// @brief Sets the callback function that is called when the client successfully connects to the broker.
    /// @param handler The callback function.
    /// @return True if the callback was set successfully, false otherwise.
    void setOnConnect(ConnectHandler handler) { onConnect_ = std::move(handler); }

    /// @brief  Adds a subscription to a topic. The client will receive messages published to this topic.
    /// @param topic The topic to subscribe to.
    /// @return True if the subscription was added successfully, false otherwise.
    void addSubscription(const std::string& topic) { subscriptions_.push_back(topic); }

    /// @brief Connects to the MQTT broker.
    /// @param host The hostname or IP address of the broker.
    /// @param port The port of the broker.
    /// @return True if the connection was successful, false otherwise.
    bool connect(const std::string& host, int port);  // asynchroon, start netwerkthread

    void disconnect();

    /// @brief Publishes a message to a topic.
    /// @param topic The topic to publish the message to.
    /// @param payload The payload of the message.
    /// @param retain Whether the message should be retained.
    /// @param qos The Quality of Service level for the message (0, 1, or 2).
    /// @return True if the message was published successfully, false otherwise.
    bool publish(const std::string& topic, const std::string& payload, bool retain = false, int qos = 0);

private:
    /// @brief The callback function that is called when the client successfully connects to the broker.
    /// @param mosq The mosquitto instance.
    /// @param self The client instance.
    /// @param rc The return code.
    static void onConnectStatic(mosquitto*, void* self, int rc);

    /// @brief The callback function that is called when a message is received on a subscribed topic.
    /// @param mosq The mosquitto instance.
    /// @param self The client instance.
    /// @param msg The received message.
    static void onMessageStatic(mosquitto*, void* self, const struct mosquitto_message* msg);

    mosquitto* mosq_ = nullptr;
    bool loopStarted_ = false;
    MessageHandler onMessage_;
    ConnectHandler onConnect_;
    std::vector<std::string> subscriptions_;
};