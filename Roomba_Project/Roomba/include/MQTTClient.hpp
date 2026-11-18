#pragma once
#include <functional>
#include <string>
#include <vector>

struct mosquitto;

// Wrapper om libmosquitto met automatisch opnieuw verbinden en heraanmelden op topics.
class MqttClient {
public:
    using MessageHandler = std::function<void(const std::string& topic, const std::string& payload)>;
    using ConnectHandler = std::function<void()>;

    explicit MqttClient(const std::string& clientId);
    ~MqttClient();
    MqttClient(const MqttClient&) = delete;
    MqttClient& operator=(const MqttClient&) = delete;

    // Moet vóór connect() worden aangeroepen.
    void setWill(const std::string& topic, const std::string& payload, bool retain);
    void setCredentials(const std::string& user, const std::string& password);
    void setOnMessage(MessageHandler handler) { onMessage_ = std::move(handler); }
    void setOnConnect(ConnectHandler handler) { onConnect_ = std::move(handler); }
    void addSubscription(const std::string& topic) { subscriptions_.push_back(topic); }

    bool connect(const std::string& host, int port);  // asynchroon, start netwerkthread
    void disconnect();
    bool publish(const std::string& topic, const std::string& payload, bool retain = false, int qos = 0);

private:
    static void onConnectStatic(mosquitto*, void* self, int rc);
    static void onMessageStatic(mosquitto*, void* self, const struct mosquitto_message* msg);

    mosquitto* mosq_ = nullptr;
    bool loopStarted_ = false;
    MessageHandler onMessage_;
    ConnectHandler onConnect_;
    std::vector<std::string> subscriptions_;
};