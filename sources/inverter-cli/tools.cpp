#include <mutex>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/time.h>
#include <string>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include "main.h"
#include "tools.h"

#include <cJSON.h>

std::mutex log_mutex;

double roundToTwoDecimalPlaces(double num)
{
    return round(num * 100.0) / 100.0;
}

void lprintf(const char *format, ...)
{
    // Only print if debug flag is set, else do nothing
    if (debugFlag)
    {
        va_list ap;
        char fmt[2048];

        // actual time
        time_t rawtime;
        struct tm *timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        char buf[256];
        strcpy(buf, asctime(timeinfo));
        buf[strlen(buf) - 1] = 0;

        // connect with args
        snprintf(fmt, sizeof(fmt), "%s %s\n", buf, format);

        // put on screen:
        va_start(ap, format);
        vprintf(fmt, ap);
        va_end(ap);

        // to the logfile:
        static FILE *log;
        log_mutex.lock();
        log = fopen(LOG_FILE, "a");
        va_start(ap, format);
        vfprintf(log, fmt, ap);
        va_end(ap);
        fclose(log);
        log_mutex.unlock();
    }
}

int print_help()
{
    printf("\nUSAGE:  ./inverter_poller <args> [-r <command>], [-h | --help], [-1 | --run-once]\n\n");

    printf("SUPPORTED ARGUMENTS:\n");
    printf("          -r <raw-command>      TX 'raw' command to the inverter\n");
    printf("          -h | --help           This Help Message\n");
    printf("          -m | --mqtt           Sends data to Home Assistant using mqtt\n");
    printf("          -1 | --run-once       Runs one iteration on the inverter, and then exits\n");
    printf("          -d                    Additional debugging\n\n");

    printf("RAW COMMAND EXAMPLES (see protocol manual for complete list):\n");
    printf("Set output source priority  POP00     (Utility first)\n");
    printf("                            POP01     (Solar first)\n");
    printf("                            POP02     (SBU)\n");
    printf("Set charger priority        PCP00     (Utility first)\n");
    printf("                            PCP01     (Solar first)\n");
    printf("                            PCP02     (Solar and utility)\n");
    printf("                            PCP03     (Solar only)\n");
    printf("Set other commands          PEa / PDa (Enable/disable buzzer)\n");
    printf("                            PEb / PDb (Enable/disable overload bypass)\n");
    printf("                            PEj / PDj (Enable/disable power saving)\n");
    printf("                            PEu / PDu (Enable/disable overload restart)\n");
    printf("                            PEx / PDx (Enable/disable backlight)\n\n");

    return 1;
}

char *replaceSpacesWithUnderscore(const char *str)
{
    int length = strlen(str);                  // Get the length of the original string
    char *newStr = (char *)malloc(length + 1); // Allocate memory for the new string (+1 for null terminator)

    if (!newStr)
        return nullptr; // Check for successful memory allocation

    for (int i = 0; i < length; ++i)
    {
        if (str[i] == ' ')
        {
            newStr[i] = '_'; // Replace space with underscore
        }
        else
        {
            newStr[i] = str[i]; // Copy the character as-is
        }
    }

    newStr[length] = '\0'; // Null-terminate the new string
    return newStr;         // Return the new string
}

void add_number_json_mqtt(cJSON *json_data, cJSON *json_mqtt_discovery, CONFIG_MQTT config_mqtt, const char *name, double number, const char *unit_of_measure, const char *hass_mdi, const char *hass_class)
{
    char *name_with_underscore = replaceSpacesWithUnderscore(name);

    // Add to json_data
    cJSON_AddNumberToObject(json_data, name_with_underscore, number);

    // Add to json_mqtt_discovery
    //---- object
    cJSON *object = cJSON_AddObjectToObject(json_mqtt_discovery, name_with_underscore);

    cJSON_AddStringToObject(object, "name", name);

    std::string unique_id = config_mqtt.device_name + "_" + std::string(name_with_underscore);
    cJSON_AddStringToObject(object, "uniq_id", unique_id.c_str());

    //---- object - device_object
    cJSON *device_object = cJSON_AddObjectToObject(object, "device");

    cJSON_AddStringToObject(device_object, "ids", config_mqtt.device_name.c_str());
    cJSON_AddStringToObject(device_object, "mf", config_mqtt.manufacturer.c_str());
    // cJSON_AddStringToObject(device_object, "mdl", config_mqtt.device_name.c_str());
    cJSON_AddStringToObject(device_object, "name", config_mqtt.device_name.c_str());
    // cJSON_AddStringToObject(device_object, "sw", "1.0");

    //--\\ object - device_object

    std::string state_topic = config_mqtt.topic + "/" + config_mqtt.device_name + "/" + std::string(name_with_underscore);
    cJSON_AddStringToObject(object, "state_topic", state_topic.c_str());

    if (strcmp(hass_class, "None") != 0)
    {
        if (strcmp(hass_class, "enum") != 0 && strcmp(hass_class, "energy") != 0)
        {
            cJSON_AddStringToObject(object, "state_class", "measurement");
        }
        cJSON_AddStringToObject(object, "device_class", hass_class);
    }

    cJSON_AddStringToObject(object, "unit_of_measurement", unit_of_measure);
    cJSON_AddStringToObject(object, "icon", hass_mdi);

    //--\\ object

    free(name_with_underscore);
}

void add_bool_json_mqtt(cJSON *json_data, cJSON *json_mqtt_discovery, CONFIG_MQTT config_mqtt, const char *name, bool boolean, const char *unit_of_measure, const char *hass_mdi)
{
    char *name_with_underscore = replaceSpacesWithUnderscore(name);

    // Add to json_data
    cJSON_AddBoolToObject(json_data, name_with_underscore, boolean);

    // Add to json_mqtt_discovery
    //---- object
    cJSON *object = cJSON_AddObjectToObject(json_mqtt_discovery, name_with_underscore);

    cJSON_AddStringToObject(object, "name", name);

    std::string unique_id = config_mqtt.device_name + "_" + std::string(name_with_underscore);
    cJSON_AddStringToObject(object, "uniq_id", unique_id.c_str());

    //---- object - device_object
    cJSON *device_object = cJSON_AddObjectToObject(object, "device");

    cJSON_AddStringToObject(device_object, "ids", config_mqtt.device_name.c_str());
    cJSON_AddStringToObject(device_object, "mf", config_mqtt.manufacturer.c_str());
    // cJSON_AddStringToObject(device_object, "mdl", config_mqtt.device_name.c_str());
    cJSON_AddStringToObject(device_object, "name", config_mqtt.device_name.c_str());
    // cJSON_AddStringToObject(device_object, "sw", "1.0");

    //--\\ object - device_object

    std::string state_topic = config_mqtt.topic + "/" + config_mqtt.device_name + "/" + std::string(name_with_underscore);
    cJSON_AddStringToObject(object, "state_topic", state_topic.c_str());

    cJSON_AddStringToObject(object, "unit_of_measurement", unit_of_measure);
    cJSON_AddStringToObject(object, "icon", hass_mdi);

    //--\\ object

    free(name_with_underscore);
}

void add_string_json_mqtt(cJSON *json_data, cJSON *json_mqtt_discovery, CONFIG_MQTT config_mqtt, const char *name, char *txt, const char *unit_of_measure, const char *hass_mdi, const char *hass_class)
{
    char *name_with_underscore = replaceSpacesWithUnderscore(name);

    // Add to json_data
    cJSON_AddStringToObject(json_data, name_with_underscore, txt);

    // Add to json_mqtt_discovery
    //---- object
    cJSON *object = cJSON_AddObjectToObject(json_mqtt_discovery, name_with_underscore);

    cJSON_AddStringToObject(object, "name", name);

    std::string unique_id = config_mqtt.device_name + "_" + std::string(name_with_underscore);
    cJSON_AddStringToObject(object, "uniq_id", unique_id.c_str());

    //---- object - device_object
    cJSON *device_object = cJSON_AddObjectToObject(object, "device");

    cJSON_AddStringToObject(device_object, "ids", config_mqtt.device_name.c_str());
    cJSON_AddStringToObject(device_object, "mf", config_mqtt.manufacturer.c_str());
    // cJSON_AddStringToObject(device_object, "mdl", config_mqtt.device_name.c_str());
    cJSON_AddStringToObject(device_object, "name", config_mqtt.device_name.c_str());
    // cJSON_AddStringToObject(device_object, "sw", "1.0");

    //--\\ object - device_object

    std::string state_topic = config_mqtt.topic + "/" + config_mqtt.device_name + "/" + std::string(name_with_underscore);
    cJSON_AddStringToObject(object, "state_topic", state_topic.c_str());

    if (strcmp(hass_class, "None") != 0)
    {
        if (strcmp(hass_class, "enum") != 0 && strcmp(hass_class, "energy") != 0)
        {
            cJSON_AddStringToObject(object, "state_class", "measurement");
        }
        cJSON_AddStringToObject(object, "device_class", hass_class);
    }

    cJSON_AddStringToObject(object, "unit_of_measurement", unit_of_measure);
    cJSON_AddStringToObject(object, "icon", hass_mdi);

    //--\\ object

    free(name_with_underscore);
}
