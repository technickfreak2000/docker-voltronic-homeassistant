#include <iostream>
#include "mqtt_tools.h"

cMQTTSub::cMQTTSub(mqtt::async_client::ptr_t client)
{
    mqttClient = client;
    
    t2 = std::thread(&cMQTTSub::run, this);
};

void cMQTTSub::run()
{
    while (true)
    {
        auto msg = mqttClient->consume_message();

        if (!msg)
        {
            if (quit_thread)
            {
                return;
            }
            continue;
        }

        std::cout << msg->get_topic() << ": " << msg->to_string() << std::endl;

        if (quit_thread)
        {
            return;
        }
    }
}