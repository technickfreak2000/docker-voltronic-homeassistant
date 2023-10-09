#ifndef ___MAIN_H
#define ___MAIN_H

#include <atomic>
#include "inverter.h"

extern bool debugFlag;

typedef struct CONFIG_MQTT
{
    string server;
    string port;
    string topic;
    string device_name;
    string manufacturer;
    string username;
    string password;
} CONFIG_MQTT;

#endif // ___MAIN_H
