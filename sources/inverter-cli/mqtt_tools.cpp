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

        auto msg = mqttClient->consume_message();

        // if (!msg)
        // {
        //     if (quit_thread)
        //     {
        //         return;
        //     }
        //     continue;
        // }

        // std::cout << msg->get_topic() << ": " << msg->to_string() << std::endl;

        lprintf("MQTT: test");
        
        std::this_thread::sleep_for(1000ms);

        if (quit_thread)
        {
            return;
        }
    }
}