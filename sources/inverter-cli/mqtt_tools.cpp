#include <iostream>
#include "mqtt_tools.h"
#include <chrono>

#include "tools.h"

cMQTTSub::cMQTTSub(mqtt::async_client::ptr_t client)
{
    mqttClient = client;
    
    t2 = std::thread(&cMQTTSub::run, this);
};

void cMQTTSub::run()
{
    while (true)
    {

        // auto msg = mqttClient->consume_message();

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