/*
 * MqttClient: a thin C++ wrapper around libmosquitto (the C client library of Mosquitto).
 *
 * What it adds on top of the raw C API:
 *   - RAII: the library and the client object are cleaned up automatically in the destructor.
 *   - Automatic reconnect with a growing delay, handled by mosquitto's own network thread.
 *   - Automatic re-subscribe to all registered topics after every (re)connect.
 *   - Callbacks as std::function, so main.cpp can pass lambdas instead of C function pointers.
 *
 * Threading: after connect(), mosquitto runs its own background thread (loop_start).
 * The onConnect/onMessage callbacks are therefore executed on THAT thread, not on the main
 * thread. mosquitto_publish() may be called safely from other threads (e.g. the BME reader).
 */

#include "MQTTClient.hpp"

#include <iostream>
#include <mosquitto.h>
#include <stdexcept>

// Creates the client. `clientId` is the name the broker sees; it must be unique per broker,
// otherwise two clients with the same id kick each other off.
MqttClient::MqttClient(const std::string& clientId) {
    // Must be called once before using the library (initialises e.g. TLS and sockets).
    mosquitto_lib_init();

    // `true` = clean session: the broker forgets this client's subscriptions when it disconnects.
    // The last argument (`this`) is the "user data" pointer that mosquitto hands back to every
    // callback; the static callbacks below use it to find their way back to this object.
    mosq_ = mosquitto_new(clientId.c_str(), true, this);
    if (!mosq_) throw std::runtime_error("mosquitto_new mislukt");

    // Register our static callbacks. They must be static (or free) functions because
    // mosquitto is a C library and cannot call non-static member functions.
    mosquitto_connect_callback_set(mosq_, &MqttClient::onConnectStatic);
    mosquitto_message_callback_set(mosq_, &MqttClient::onMessageStatic);

    // If the connection drops, mosquitto retries automatically:
    // first after 2 s, then with exponential back-off (`true`) up to a maximum of 30 s.
    mosquitto_reconnect_delay_set(mosq_, 2, 30, true);
}

// Cleans up in the reverse order of construction.
MqttClient::~MqttClient() {
    disconnect();                                // stop the network thread first
    if (mosq_) mosquitto_destroy(mosq_);         // free the client object
    mosquitto_lib_cleanup();                     // free library-wide resources
}

// Sets the "last will and testament": a message the BROKER publishes on our behalf if this
// program disappears without a clean disconnect (crash, power loss, network failure).
// We use it to turn the status topic to "offline". Must be called BEFORE connect().
// QoS 1 = delivered at least once; `retain` keeps the message for new subscribers.
void MqttClient::setWill(const std::string& topic, const std::string& payload, bool retain) {
    mosquitto_will_set(mosq_, topic.c_str(), static_cast<int>(payload.size()), payload.data(), 1, retain);
}

// Sets the login for brokers that require authentication.
// An empty password is passed as nullptr, meaning "username only".
void MqttClient::setCredentials(const std::string& user, const std::string& password) {
    mosquitto_username_pw_set(mosq_, user.c_str(), password.empty() ? nullptr : password.c_str());
}

// Starts the connection. This returns immediately: "async" means the actual TCP connect
// happens in the background thread started by loop_start().
// The final argument (30) is the keep-alive interval in seconds: the client pings the broker
// at least this often so that a dead connection is noticed.
// Returns false if starting failed; a broker that is simply unreachable is NOT an error here,
// mosquitto keeps retrying (see the reconnect delay set in the constructor).
bool MqttClient::connect(const std::string& host, int port) {
    int rc = mosquitto_connect_async(mosq_, host.c_str(), port, 30);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "MQTT connect fout: " << mosquitto_strerror(rc) << "\n";
        return false;
    }

    // Starts the background network thread that handles sending, receiving,
    // keep-alives, reconnecting and calling our callbacks.
    rc = mosquitto_loop_start(mosq_);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "MQTT loop_start fout: " << mosquitto_strerror(rc) << "\n";
        return false;
    }
    loopStarted_ = true;
    return true;
}

// Disconnects cleanly and stops the background thread. Safe to call multiple times:
// the guard returns early if we never started, or already stopped.
// A clean disconnect means the broker does NOT publish the last will.
void MqttClient::disconnect() {
    if (!mosq_ || !loopStarted_) return;
    mosquitto_disconnect(mosq_);
    mosquitto_loop_stop(mosq_, false);   // `false` = wait for the thread to finish normally
    loopStarted_ = false;
}

// Publishes a message. `qos` is the delivery guarantee (0 = at most once, 1 = at least once,
// 2 = exactly once); `retain` makes the broker store the message and hand it to new subscribers.
// The call only queues the message for the network thread, so it returns quickly.
// A `true` result means "accepted for sending", not "received by the broker".
bool MqttClient::publish(const std::string& topic, const std::string& payload, bool retain, int qos) {
    int rc = mosquitto_publish(mosq_, nullptr, topic.c_str(), static_cast<int>(payload.size()),
                               payload.data(), qos, retain);
    return rc == MOSQ_ERR_SUCCESS;
}

// Called by mosquitto (on its network thread) when the broker answers our connect request.
// `rc` is the broker's result code: 0 = accepted, anything else = refused
// (e.g. 4 = bad username/password, 5 = not authorised).
// Being static, it receives the MqttClient through the `self` pointer given in the constructor.
void MqttClient::onConnectStatic(mosquitto*, void* self, int rc) {
    auto* c = static_cast<MqttClient*>(self);
    if (rc != 0) {
        std::cerr << "MQTT verbinding geweigerd, rc=" << rc << "\n";
        return;
    }
    std::cout << "MQTT verbonden\n";

    // Subscribe again on every connect. With a clean session the broker forgets our
    // subscriptions after a disconnect, so this also covers automatic reconnects.
    for (const auto& t : c->subscriptions_) mosquitto_subscribe(c->mosq_, nullptr, t.c_str(), 1);

    // Let the application react, e.g. by publishing "online" on the status topic.
    if (c->onConnect_) c->onConnect_();
}

// Called by mosquitto (on its network thread) for every incoming message on a subscribed topic.
// Converts the raw C data to std::string and passes it on to the registered handler.
void MqttClient::onMessageStatic(mosquitto*, void* self, const mosquitto_message* msg) {
    auto* c = static_cast<MqttClient*>(self);
    if (!c->onMessage_ || !msg->topic) return;   // nobody listening, or a malformed message

    // The payload is raw bytes with an explicit length (it is NOT null-terminated),
    // so the length must be given. For an empty message payload may be NULL with length 0.
    std::string payload(static_cast<const char*>(msg->payload), static_cast<size_t>(msg->payloadlen));
    c->onMessage_(msg->topic, payload);
}