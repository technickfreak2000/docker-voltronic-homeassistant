#include <iostream>
#include "mqtt_tools.h"
#include <chrono>

#include "tools.h"
using namespace std::chrono_literals;

cMQTTSub::cMQTTSub(mqtt::async_client::ptr_t client)
{
    mqttClient = client;
    
    t2 = std::thread(&cMQTTSub::run, this);
};

void cMQTTSub::run()
{
    while (true)
    {
        if (!mqttClient) {
            std::cerr << "Error: mqttClient is not initialized!" << std::endl;
            return;
        }

        if (!mqttClient->is_connected()) {
            std::cerr << "MQTT client is not connected!" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        auto msg = mqttClient->consume_message();

        if (!msg) {
            if (quit_thread) {
                return;
            }
            continue;  // No message received, skip to the next loop iteration
        }

        std::cout << msg->get_topic() << ": " << msg->to_string() << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        if (quit_thread) {
            return;
        }
    }
}