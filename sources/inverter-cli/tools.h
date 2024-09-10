#include <cJSON.h>
#include <atomic>
#include <cstring>

#ifndef ___TOOLS_H
#define ___TOOLS_H

typedef struct CONFIG_MQTT
{
    std::string server;
    std::string port;
    std::string topic;
    std::string device_name;
    std::string manufacturer;
    std::string username;
    std::string password;
} CONFIG_MQTT;

#define LOG_FILE "/dev/null"

double roundToTwoDecimalPlaces(double num);

void lprintf(const char *format, ...);
int print_help();

char *replaceSpacesWithUnderscore(const char *str);
void add_number_json_mqtt(cJSON *json_data, cJSON *json_mqtt_discovery, CONFIG_MQTT config_mqtt, const char *name, double number, const char *unit_of_measure, const char *hass_mdi, const char *hass_class);

#endif // ___TOOLS_H
