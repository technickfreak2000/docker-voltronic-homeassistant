#ifndef ___MQTT_TOOLS_H
#define ___MQTT_TOOLS_H

#include <iostream>
#include <cstdlib>
#include <string.h>
#include <cstring>
#include <cctype>
#include <thread>
#include <chrono>
#include "mqtt/async_client.h"
#include <atomic>

const int QOS = 1;
const int N_RETRY_ATTEMPTS = 5;

// Callbacks for the success or failures of requested actions.
// This could be used to initiate further action, but here we just log the
// results to the console.

class action_listener : public virtual mqtt::iaction_listener
{
    std::string name_;

    void on_failure(const mqtt::token &tok) override
    {
        std::cout << name_ << " failure";
        if (tok.get_message_id() != 0)
            std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
        std::cout << std::endl;
    }

    void on_success(const mqtt::token &tok) override
    {
        std::cout << name_ << " success";
        if (tok.get_message_id() != 0)
            std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
        auto top = tok.get_topics();
        if (top && !top->empty())
            std::cout << "\ttoken topic: '" << (*top)[0] << "', ..." << std::endl;
        std::cout << std::endl;
    }

public:
    action_listener(const std::string &name) : name_(name) {}
};

class PubSubCallback : public virtual mqtt::callback
    {
    public:
        void connection_lost(const std::string& cause) override
        {
            std::cout << "Connection lost: " << cause << std::endl;
        }

        void delivery_complete(mqtt::delivery_token_ptr token) override
        {
            std::cout << "Message delivered" << std::endl;
        }

        void message_arrived(mqtt::const_message_ptr message) override
        {
            std::cout << "Message arrived: " << message->get_payload_str() << std::endl;
        }

    };

/**
 * Local callback & listener class for use with the client connection.
 * This is primarily intended to receive messages, but it will also monitor
 * the connection to the broker. If the connection is lost, it will attempt
 * to restore the connection and re-subscribe to the topic.
 */
class callback : public virtual mqtt::callback,
                 public virtual mqtt::iaction_listener

{
    // Counter for the number of connection retries
    int nretry_;
    // The MQTT client
    mqtt::async_client &cli_;
    // Options to use if we need to reconnect
    mqtt::connect_options &connOpts_;
    // An action listener to display the result of actions.
    action_listener subListener_;

    std::string *mqtt_topic_;
    std::string *mqtt_client_id_;

    // This deomonstrates manually reconnecting to the broker by calling
    // connect() again. This is a possibility for an application that keeps
    // a copy of it's original connect_options, or if the app wants to
    // reconnect with different options.
    // Another way this can be done manually, if using the same options, is
    // to just call the async_client::reconnect() method.
    void reconnect()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
        try
        {
            cli_.connect(connOpts_, nullptr, *this);
        }
        catch (const mqtt::exception &exc)
        {
            std::cerr << "Error: " << exc.what() << std::endl;
            exit(1);
        }
    }

    // Re-connection failure
    void on_failure(const mqtt::token &tok) override
    {
        std::cout << "Connection attempt failed" << std::endl;
        if (++nretry_ > N_RETRY_ATTEMPTS)
            exit(1);
        reconnect();
    }

    // (Re)connection success
    // Either this or connected() can be used for callbacks.
    void on_success(const mqtt::token &tok) override {}

    // (Re)connection success
    void connected(const std::string &cause) override
    {
        std::cout << "\nConnection success" << std::endl;
        std::cout << "\nSubscribing to topic '" << *mqtt_topic_ << "'\n"
                  << "\tfor client " << *mqtt_client_id_
                  << " using QoS" << QOS << "\n"
                  << "\nPress Q<Enter> to quit\n"
                  << std::endl;

        cli_.subscribe(*mqtt_topic_, QOS, nullptr, subListener_);
    }

    // Callback for when the connection is lost.
    // This will initiate the attempt to manually reconnect.
    void connection_lost(const std::string &cause) override
    {
        std::cout << "\nConnection lost" << std::endl;
        if (!cause.empty())
            std::cout << "\tcause: " << cause << std::endl;

        std::cout << "Reconnecting..." << std::endl;
        nretry_ = 0;
        reconnect();
    }

    // Callback for when a message arrives.
    void message_arrived(mqtt::const_message_ptr msg) override
    {
        std::cout << "Message arrived" << std::endl;
        std::cout << "\ttopic: '" << msg->get_topic() << "'" << std::endl;
        std::cout << "\tpayload: '" << msg->to_string() << "'\n"
                  << std::endl;
    }

    void delivery_complete(mqtt::delivery_token_ptr token) override {}

public:
    callback(mqtt::async_client &cli, mqtt::connect_options &connOpts, std::string *mqtt_topic, std::string *mqtt_client_id)
        : nretry_(0), cli_(cli), connOpts_(connOpts), subListener_("Subscription"), mqtt_topic_(mqtt_topic), mqtt_client_id_(mqtt_client_id) {}
};

class cMQTTSub
{
  std::mutex m;
  std::thread t2;
  std::atomic_bool quit_thread{false};
  mqtt::async_client::ptr_t mqttClient;

  void run();

public:
  cMQTTSub(mqtt::async_client::ptr_t mqttClient);
  bool runOnce = false;

  mqtt::async_client::ptr_t getClient()
  {
    return mqttClient;
  }

  void terminateThread()
  {
    mqttClient->stop_consuming();
    quit_thread = true;
    t2.join();
  }
  
};

#endif // ___MQTT_TOOLS_H
