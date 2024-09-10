// Lightweight program to take the sensor data from a Voltronic Axpert, Mppsolar PIP, Voltacon, Effekta, and other branded OEM Inverters and send it to a MQTT server for ingestion...
// Adapted from "Maio's" C application here: https://skyboo.net/2017/03/monitoring-voltronic-power-axpert-mex-inverter-under-linux/
//
// Please feel free to adapt this code and add more parameters -- See the following forum for a breakdown on the RS323 protocol: http://forums.aeva.asn.au/viewtopic.php?t=4332
// ------------------------------------------------------------------------

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <sys/file.h>

#include "main.h"
#include "tools.h"
#include "inputparser.h"

#include <pthread.h>
#include <signal.h>
#include <string.h>

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>

#include <cJSON.h>

#include <cstdlib>
#include <cstring>
#include <cctype>
#include <chrono>
#include <memory>
#include <mqtt/async_client.h>
#include "mqtt_tools.h"

cJSON *json_discovery_old = NULL;

bool mqtt_sel = false;
bool debugFlag = false;
bool runOnce = false;
cInverter *inv = NULL;
cMQTTSub *mqttSub = NULL;

// ---------------------------------------
// Global configs read from 'inverter.conf'

string device_connection;
float ampfactor;
float wattfactor;

// MQTT
CONFIG_MQTT config_mqtt;

// ---------------------------------------

void attemptAddSetting(int *addTo, string addFrom)
{
    try
    {
        *addTo = stof(addFrom);
    }
    catch (exception e)
    {
        cout << e.what() << '\n';
        cout << "There's probably a string in the settings file where an int should be.\n";
    }
}

void attemptAddSetting(float *addTo, string addFrom)
{
    try
    {
        *addTo = stof(addFrom);
    }
    catch (exception e)
    {
        cout << e.what() << '\n';
        cout << "There's probably a string in the settings file where a floating point should be.\n";
    }
}

void getSettingsFile(string filename)
{
    try
    {
        string fileline, linepart1, linepart2;
        ifstream infile;
        infile.open(filename);
        while (!infile.eof())
        {
            getline(infile, fileline);
            size_t firstpos = fileline.find("#");
            if (firstpos != 0 && fileline.length() != 0)
            { // Ignore lines starting with # (comment lines)
                size_t delimiter = fileline.find("=");
                linepart1 = fileline.substr(0, delimiter);
                linepart2 = fileline.substr(delimiter + 1, string::npos - delimiter);

                if (linepart1 == "device")
                    device_connection = linepart2;
                else if (linepart1 == "server")
                    config_mqtt.server = linepart2;
                else if (linepart1 == "port")
                    config_mqtt.port = linepart2;
                else if (linepart1 == "topic")
                    config_mqtt.topic = linepart2;
                else if (linepart1 == "devicename")
                    config_mqtt.device_name = linepart2;
                else if (linepart1 == "manufacturer")
                    config_mqtt.manufacturer = linepart2;
                else if (linepart1 == "username")
                    config_mqtt.username = linepart2;
                else if (linepart1 == "password")
                    config_mqtt.password = linepart2;
                else if (linepart1 == "amperage_factor")
                    attemptAddSetting(&ampfactor, linepart2);
                else if (linepart1 == "watt_factor")
                    attemptAddSetting(&wattfactor, linepart2);
                else
                    continue;
            }
        }
        infile.close();
        if (mqtt_sel)
        {
            lprintf("MQTT: Settings loaded");
            lprintf("MQTT: Server: %s", config_mqtt.server.c_str());
            lprintf("MQTT: Port: %s", config_mqtt.port.c_str());
            lprintf("MQTT: Topic: %s", config_mqtt.topic.c_str());
            lprintf("MQTT: Devicename: %s", config_mqtt.device_name.c_str());
            lprintf("MQTT: Manufacturer: %s", config_mqtt.manufacturer.c_str());
            lprintf("MQTT: Username: %s", config_mqtt.username.c_str());
            lprintf("MQTT: Password: %s", config_mqtt.password.c_str());
        }
    }
    catch (...)
    {
        cout << "Settings could not be read properly...\n";
    }
}

int main(int argc, char *argv[])
{
    // Get command flag settings from the arguments (if any)
    InputParser cmdArgs(argc, argv);
    const string &rawcmd = cmdArgs.getCmdOption("-r");
    if (cmdArgs.cmdOptionExists("-h") || cmdArgs.cmdOptionExists("--help"))
    {
        return print_help();
    }
    if (cmdArgs.cmdOptionExists("-m") || cmdArgs.cmdOptionExists("--mqtt"))
    {
        mqtt_sel = true;
    }
    if (cmdArgs.cmdOptionExists("-d"))
    {
        debugFlag = true;
    }
    if (cmdArgs.cmdOptionExists("-1") || cmdArgs.cmdOptionExists("--run-once"))
    {
        runOnce = true;
    }
    lprintf("INVERTER: Debug set");
    const char *settings;

    // Get the rest of the settings from the conf file
    if (access("./inverter.conf", F_OK) != -1)
    { // file exists
        settings = "./inverter.conf";
    }
    else
    { // file doesn't exist
        settings = "/etc/inverter/inverter.conf";
    }
    getSettingsFile(settings);

    int fd = open(settings, O_RDWR);
    while (flock(fd, LOCK_EX))
        sleep(1);

    inv = new cInverter(device_connection);
    // Logic to send 'raw commands' to the inverter..
    if (!rawcmd.empty())
    {
        // Execute and return
        printf("Reply:  %s\n", inv->ExecuteCmd(rawcmd)->c_str());
        exit(0);
    }

    if (runOnce)
    {
        inv->runOnce = true;
        inv->poll();
    }
    else
        inv->runMultiThread();

    mqtt::async_client::ptr_t client = NULL;
    if (mqtt_sel)
    {
        string address = "tcp://" + string(config_mqtt.server) + ":" + string(config_mqtt.port);
        string client_id = config_mqtt.device_name;
        lprintf("DEBUG:  Starting mqtt client...");
        client = std::make_shared<mqtt::async_client>(address, client_id);

        mqtt::connect_options connOpts;
        connOpts.set_keep_alive_interval(20);
        connOpts.set_clean_session(true);
        connOpts.set_user_name(config_mqtt.username);
        connOpts.set_password(config_mqtt.password);

        try
        {
            auto rsp = client->connect(connOpts)->get_connect_response();

            mqtt::token_ptr subToken = client->subscribe("test2", QOS);
            subToken->wait();

            // mqttSub = new cMQTTSub(client);
        }
        catch (const mqtt::exception &ex)
        {
            std::cerr << "MQTT Exception: " << ex.what() << ex.get_error_str() << std::endl;
            return 1;
        }
    }
    while (true)
    {
        lprintf("DEBUG:  Start loop");

        // Once we receive all queries print it to screen
        if (inv->inv_data_avail)
        {
            QMN *qmn = &inv->qmn;
            QID *qid = &inv->qid;
            QMOD *qmod = &inv->qmod;
            QFLAG *qflag = &inv->qflag;
            QVFWn *qvfwn = &inv->qvfwn;
            QPIGSn *qpigsn = &inv->qpigsn;
            QPGSn *qpgsn = &inv->qpgsn;
            QPIRI *qpiri = &inv->qpiri;
            QPIWS *qpiws = &inv->qpiws;

            int counter = 0;
            char *combined_query = (char *)malloc(50 * sizeof(char));
            ;

            if (qmn->model_name != NULL)
            {
                lprintf("INVERTER: Model name: %s", qmn->model_name);
            }

            if (qmod->inverter_mode_int != 0)
            {
                lprintf("INVERTER: Mode currently set to: %d", qmod->inverter_mode_int);
            }

            cJSON *json = cJSON_CreateObject();
            cJSON *json_discovery = cJSON_CreateObject();

            // Here the json gets build, you can customize the outbut based on the first inverter model name

            // QMN
            add_string_json_mqtt(json, json_discovery, config_mqtt, "Inverter Model Name", qmn->model_name, "", "mdi:power-plug", "enum");

            // QID
            add_string_json_mqtt(json, json_discovery, config_mqtt, "Inverter ID", qid->inverter_id, "", "mdi:identifier", "None");

            // QMOD
            add_number_json_mqtt(json, json_discovery, config_mqtt, "Inverter Mode", qmod->inverter_mode_int, "", "mdi:solar-power", "enum");

            // QFLAG
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Silence Open Buzzer", qflag->silence_open_buzzer, "", "mdi:bell-off");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Bypass Function", qflag->bypass_function, "", "mdi:flash");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Bypass Function Forbidden", qflag->bypass_function_forbidden, "", "mdi:flash-off");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Power Saving", qflag->power_saving, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "LCD Timeout Default Page", qflag->lcd_timeout_default_page, "", "mdi:timer");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Overload Restart", qflag->overload_restart, "", "mdi:restart");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Overtemperature Restart", qflag->overtemperature_restart, "", "mdi:thermometer");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "LCD Backlight", qflag->lcd_backlight, "", "mdi:brightness-6");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Alarm Primary Input", qflag->alarm_primary_input, "", "mdi:alarm");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Fault Code Record", qflag->fault_code_record, "", "mdi:note-text");

            // QVFWn
            QVFWn *current_qvfwn = qvfwn;
            while (current_qvfwn != NULL)
            {
                sprintf(combined_query, "Fw_version_%d", counter);
                cJSON_AddStringToObject(json, combined_query, current_qvfwn->fw_version);

                counter++;
                current_qvfwn = current_qvfwn->next;
            }

            // QPIGS
            QPIGSn *current_qpigsn = qpigsn;
            counter = 0;
            while (current_qpigsn != NULL)
            {
                sprintf(combined_query, "SCC %d AC Grid Frequency", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->freq_grid, "Hz", "mdi:current-ac", "frequency");

                sprintf(combined_query, "SCC %d AC Out Voltage", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->voltage_out, "V", "mdi:power-plug", "voltage");

                sprintf(combined_query, "SCC %d AC Out Frequency", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->freq_out, "Hz", "mdi:current-ac", "frequency");

                sprintf(combined_query, "SCC %d PV In Voltage", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->pv_input_voltage, "V", "mdi:solar-panel-large", "voltage");

                sprintf(combined_query, "SCC %d PV In Current", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->pv_input_current, "A", "mdi:solar-panel-large", "current");

                sprintf(combined_query, "SCC %d PV In Watts", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->pv_input_watts, "W", "mdi:solar-panel-large", "power");
                // sprintf(combined_query, "SCC_%d_PV_in_watthour", counter);
                // cJSON_AddNumberToObject(json, combined_query, current_qpigsn->pv_input_watthour);
                sprintf(combined_query, "SCC %d SCC Voltage", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->scc_voltage, "V", "mdi:current-dc", "voltage");

                sprintf(combined_query, "SCC %d Load Pct", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->load_percent, "%", "mdi:brightness-percent", "None");

                sprintf(combined_query, "SCC %d Load Watt", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->load_watt, "W", "mdi:chart-bell-curve", "power");

                sprintf(combined_query, "SCC %d Load Va", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->load_va, "VA", "mdi:chart-bell-curve", "apparent_power");
                // sprintf(combined_query, "SCC_%d_Load_watthour", counter);
                // cJSON_AddNumberToObject(json, combined_query, current_qpigsn->load_watthour);
                sprintf(combined_query, "SCC %d Bus Voltage", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->voltage_bus, "V", "mdi:details", "voltage");

                sprintf(combined_query, "SCC %d Heatsink Temperature", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->temp_heatsink, "°C", "mdi:details", "temperature");

                sprintf(combined_query, "SCC %d Battery Capacity", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->batt_capacity, "%", "mdi:battery-outline", "battery");

                sprintf(combined_query, "SCC %d Battery Voltage", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->voltage_batt, "V", "mdi:battery-outline", "voltage");

                sprintf(combined_query, "SCC %d Battery Charge Current", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->batt_charge_current, "A", "mdi:current-dc", "current");

                sprintf(combined_query, "SCC %d Battery Discharge Current", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->batt_discharge_current, "A", "mdi:current-dc", "current");

                sprintf(combined_query, "SCC %d Load Status On", counter);
                add_bool_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->load_status, "", "mdi:power");

                sprintf(combined_query, "SCC %d SCC Charge On", counter);
                add_bool_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->charging_status_scc, "", "mdi:power");

                sprintf(combined_query, "SCC %d AC Charge On", counter);
                add_bool_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->charging_status_ac, "", "mdi:power");

                sprintf(combined_query, "SCC %d Battery Voltage Offset For Fans On", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->battery_voltage_offset_for_fans_on, "10mV", "mdi:battery-outline", "voltage");

                sprintf(combined_query, "SCC %d Eeprom Version", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->eeprom_version, "", "mdi:power", "None");

                sprintf(combined_query, "SCC %d PV Charging Power", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->pv_charging_power, "W", "mdi:solar-panel-large", "power");

                sprintf(combined_query, "SCC %d Charging To Floating Mode", counter);
                add_bool_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->charging_to_floating_mode, "", "mdi:solar-power");

                sprintf(combined_query, "SCC %d Switch On", counter);
                add_bool_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->switch_on, "", "mdi:power");

                sprintf(combined_query, "SCC %d Dustproof Installed", counter);
                add_bool_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpigsn->dustproof_installed, "", "mdi:solar-power");

                counter++;
                current_qpigsn = current_qpigsn->next;
            }

            // QPGS
            QPGSn *current_qpgsn = qpgsn;
            counter = 0;
            while (current_qpgsn != NULL)
            {
                sprintf(combined_query, "INV %d Inverter ID", counter);
                add_string_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->inverter_id, "", "mdi:power", "None");

                sprintf(combined_query, "INV %d Inverter Mode", counter);
                add_string_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->inverter_mode, "", "mdi:solar-power", "enum");

                sprintf(combined_query, "INV %d Inverter Mode Int", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->inverter_mode_int, "", "mdi:solar-power", "None");

                sprintf(combined_query, "INV %d Fault Code", counter);
                add_string_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->fault_code, "", "mdi:power", "None");

                sprintf(combined_query, "INV %d Voltage Grid", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->voltage_grid, "V", "mdi:power-plug", "voltage");

                sprintf(combined_query, "INV %d Freq Grid", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->freq_grid, "Hz", "mdi:current-ac", "frequency");

                sprintf(combined_query, "INV %d Voltage Out", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->voltage_out, "V", "mdi:power-plug", "voltage");

                sprintf(combined_query, "INV %d Freq Out", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->freq_out, "Hz", "mdi:current-ac", "frequency");

                sprintf(combined_query, "INV %d Load VA", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->load_va, "VA", "mdi:chart-bell-curve", "apparent_power");

                sprintf(combined_query, "INV %d Load Watt", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->load_watt, "W", "mdi:chart-bell-curve", "power");

                sprintf(combined_query, "INV %d Load Percent", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->load_percent, "%", "mdi:brightness-percent", "None");

                sprintf(combined_query, "INV %d Voltage Batt", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->voltage_batt, "V", "mdi:battery-outline", "voltage");

                sprintf(combined_query, "INV %d Batt Charge Current", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->batt_charge_current, "A", "mdi:current-dc", "current");

                sprintf(combined_query, "INV %d Batt Capacity", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->batt_capacity, "%", "mdi:battery-outline", "battery");

                sprintf(combined_query, "INV %d PV Input Voltage", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->pv_input_voltage, "V", "mdi:solar-panel-large", "voltage");

                sprintf(combined_query, "INV %d Batt Charge Current Total", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->batt_charge_current_total, "A", "mdi:current-dc", "current");

                sprintf(combined_query, "INV %d Load VA Total", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->load_va_total, "VA", "mdi:chart-bell-curve", "apparent_power");

                sprintf(combined_query, "INV %d Load Watt Total", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->load_watt_total, "W", "mdi:chart-bell-curve", "power");

                sprintf(combined_query, "INV %d Load Percent Total", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->load_percent_total, "%", "mdi:brightness-percent", "None");

                sprintf(combined_query, "INV %d SCC OK", counter);
                add_bool_json_mqtt(json, json_discovery, config_mqtt, combined_query, atomic_load(&current_qpgsn->scc_ok), "", "mdi:power");

                sprintf(combined_query, "INV %d Charging Status AC", counter);
                add_bool_json_mqtt(json, json_discovery, config_mqtt, combined_query, atomic_load(&current_qpgsn->charging_status_ac), "", "mdi:power");

                sprintf(combined_query, "INV %d Charging Status SCC", counter);
                add_bool_json_mqtt(json, json_discovery, config_mqtt, combined_query, atomic_load(&current_qpgsn->charging_status_scc), "", "mdi:power");

                sprintf(combined_query, "INV %d Battery Open", counter);
                add_bool_json_mqtt(json, json_discovery, config_mqtt, combined_query, atomic_load(&current_qpgsn->battery_open), "", "mdi:power");

                sprintf(combined_query, "INV %d Battery Under", counter);
                add_bool_json_mqtt(json, json_discovery, config_mqtt, combined_query, atomic_load(&current_qpgsn->battery_under), "", "mdi:power");

                sprintf(combined_query, "INV %d Battery Normal", counter);
                add_bool_json_mqtt(json, json_discovery, config_mqtt, combined_query, atomic_load(&current_qpgsn->battery_normal), "", "mdi:power");

                sprintf(combined_query, "INV %d AC Loss", counter);
                add_bool_json_mqtt(json, json_discovery, config_mqtt, combined_query, atomic_load(&current_qpgsn->ac_loss), "", "mdi:power");

                sprintf(combined_query, "INV %d AC OK", counter);
                add_bool_json_mqtt(json, json_discovery, config_mqtt, combined_query, atomic_load(&current_qpgsn->ac_ok), "", "mdi:power");

                sprintf(combined_query, "INV %d Configuration Changed", counter);
                add_bool_json_mqtt(json, json_discovery, config_mqtt, combined_query, atomic_load(&current_qpgsn->configuration_changed), "", "mdi:power");

                sprintf(combined_query, "INV %d Output Mode", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->output_mode, "", "mdi:solar-power", "None");

                sprintf(combined_query, "INV %d Charger Source Priority", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->charger_source_priority, "", "mdi:solar-power", "None");

                sprintf(combined_query, "INV %d Max Charger Current", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->max_charger_current, "A", "mdi:current-ac", "current");

                sprintf(combined_query, "INV %d Max Charger Range", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->max_charger_range, "", "mdi:solar-power", "None");

                sprintf(combined_query, "INV %d Max AC Charger Current", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->max_ac_charger_current, "A", "mdi:current-ac", "current");

                sprintf(combined_query, "INV %d PV Input Current", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->pv_input_current, "A", "mdi:solar-panel-large", "current");

                sprintf(combined_query, "INV %d Batt Discharge Current", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->batt_discharge_current, "A", "mdi:current-dc", "current");

                sprintf(combined_query, "INV %d PV Input Watts", counter);
                add_number_json_mqtt(json, json_discovery, config_mqtt, combined_query, current_qpgsn->pv_input_watts, "W", "mdi:solar-panel-large", "power");

                counter++;
                current_qpgsn = current_qpgsn->next;
            }

            // QPIRI
            // Adding numbers to JSON with MQTT parameters
            add_number_json_mqtt(json, json_discovery, config_mqtt, "Battery Recharge Voltage", qpiri->batt_recharge_voltage, "V", "mdi:current-dc", "voltage");
            add_number_json_mqtt(json, json_discovery, config_mqtt, "Battery Under Voltage", qpiri->batt_under_voltage, "V", "mdi:current-dc", "voltage");
            add_number_json_mqtt(json, json_discovery, config_mqtt, "Battery Bulk Voltage", qpiri->batt_bulk_voltage, "V", "mdi:current-dc", "voltage");
            add_number_json_mqtt(json, json_discovery, config_mqtt, "Battery Float Voltage", qpiri->batt_float_voltage, "V", "mdi:current-dc", "voltage");
            add_number_json_mqtt(json, json_discovery, config_mqtt, "Max Grid Charge Current", qpiri->max_grid_charge_current, "A", "mdi:current-ac", "current");
            add_number_json_mqtt(json, json_discovery, config_mqtt, "Max Charge Current", qpiri->max_charge_current, "A", "mdi:current-ac", "current");
            add_number_json_mqtt(json, json_discovery, config_mqtt, "Out Source Priority", qpiri->out_source_priority, "", "mdi:grid", "None");
            add_number_json_mqtt(json, json_discovery, config_mqtt, "Charger Source Priority", qpiri->charger_source_priority, "", "mdi:solar-power", "None");
            add_number_json_mqtt(json, json_discovery, config_mqtt, "Battery Redischarge Voltage", qpiri->batt_redischarge_voltage, "V", "mdi:battery-negative", "voltage");

            // QPIWS
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Reserved", qpiws->reserved, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Inverter Fault", qpiws->inverter_fault, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Bus Over", qpiws->bus_over, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Bus Under", qpiws->bus_under, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Bus Soft Fail", qpiws->bus_soft_fail, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Line Fail", qpiws->line_fail, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "OPV Short", qpiws->opv_short, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Inverter Voltage Too Low", qpiws->inverter_voltage_too_low, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Inverter Voltage Too High", qpiws->inverter_voltage_too_high, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Over Temperature", qpiws->over_temperature, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Fan Locked", qpiws->fan_locked, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Battery Voltage High", qpiws->battery_voltage_high, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Battery Low Alarm", qpiws->battery_low_alarm, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Overcharge", qpiws->overcharge, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Battery Under Shutdown", qpiws->battery_under_shutdown, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Battery Derating", qpiws->battery_derating, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Over Load", qpiws->over_load, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "EEPROM Fault", qpiws->eeprom_fault, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Inverter Over Current", qpiws->inverter_over_current, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Inverter Soft Fail", qpiws->inverter_soft_fail, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Self Test Fail", qpiws->self_test_fail, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "OP DC Voltage Over", qpiws->op_dc_voltage_over, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Battery Open", qpiws->bat_open, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Current Sensor Fail", qpiws->current_sensor_fail, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Battery Short", qpiws->battery_short, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Power Limit", qpiws->power_limit, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "PV Voltage High", qpiws->pv_voltage_high, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "MPPT Overload Fault", qpiws->mppt_overload_fault, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "MPPT Overload Warning", qpiws->mppt_overload_warning, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Battery Too Low to Charge", qpiws->battery_too_low_to_charge, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "DC DC Over Current", qpiws->dc_dc_over_current, "", "mdi:power");
            add_bool_json_mqtt(json, json_discovery, config_mqtt, "Reserved2", qpiws->reserved2, "", "mdi:power");

            free(combined_query);

            char *jsonString = cJSON_Print(json);

            if (mqtt_sel)
            {
                char *json_discoveryString = cJSON_Print(json_discovery);
                char *json_discovery_oldString = NULL;
                if (json_discovery_old != NULL)
                {
                    json_discovery_oldString = cJSON_Print(json_discovery_old);
                }

                if (json_discovery_oldString == NULL || strcmp(json_discoveryString, json_discovery_oldString) != 0)
                {
                    lprintf("MQTT: Sending discovery");

                    cJSON *json_key = nullptr;
                    cJSON_ArrayForEach(json_key, json_discovery)
                    {
                        const char *key = json_key->string; // Get the key

                        char *json_message = cJSON_PrintUnformatted(json_key);

                        // Construct MQTT topic and publish message
                        std::string topic = "homeassistant/sensor/" + std::string(config_mqtt.device_name) + "/" + std::string(key) + "/config";
                        mqtt::message_ptr pubMessage = mqtt::make_message(topic, std::string(json_message), QOS, false);
                        client->publish(pubMessage)->wait();

                        cJSON_free(json_message);
                    }

                    lprintf("MQTT: Discovery JSON: ");
                    lprintf("%s\n", json_discoveryString);

                    if (json_discovery_old != NULL)
                    {
                        cJSON_Delete(json_discovery_old);
                    }
                    json_discovery_old = json_discovery;
                }
                cJSON_free(json_discoveryString);
                if (json_discovery_oldString != NULL)
                {
                    cJSON_free(json_discovery_oldString);
                }

                lprintf("MQTT: Sending keys");

                cJSON *json_key = nullptr;
                cJSON_ArrayForEach(json_key, json)
                {
                    const char *key = json_key->string; // Get the key
                    std::string value_str;

                    if (cJSON_IsString(json_key))
                    {
                        value_str = json_key->valuestring;
                        if (value_str == "true")
                        {
                            value_str = "1";
                        }
                        else if (value_str == "false")
                        {
                            value_str = "0";
                        }
                    }
                    else if (cJSON_IsBool(json_key))
                    {
                        value_str = cJSON_IsTrue(json_key) ? "1" : "0";
                    }
                    else if (cJSON_IsNumber(json_key))
                    {
                        value_str = std::to_string(json_key->valuedouble);
                    }
                    else
                    {
                        continue; // Skip other types
                    }

                    // Construct MQTT topic and publish message
                    std::string topic = std::string(config_mqtt.topic) + "/" + std::string(config_mqtt.device_name) + "/" + std::string(key);
                    mqtt::message_ptr pubMessage = mqtt::make_message(topic, std::string(value_str), QOS, false);
                    client->publish(pubMessage)->wait();
                }

                lprintf("MQTT: Sending json");

                // Send raw json
                std::string topic = std::string(config_mqtt.topic) + "/" + std::string(config_mqtt.device_name) + "/json";
                mqtt::message_ptr pubMessage = mqtt::make_message(topic, std::string(jsonString), QOS, false);
                client->publish(pubMessage)->wait();
            }

            printf("%s\n", jsonString);

            cJSON_free(jsonString);
            cJSON_Delete(json);

            if (runOnce)
            {
                // there is no thread -- ups->terminateThread();
                // Do once and exit instead of loop endlessly
                if (mqtt_sel)
                {
                    mqtt::async_client::ptr_t client = mqttSub->getClient();
                    mqttSub->terminateThread();

                    mqtt::token_ptr disconnectionToken = client->disconnect();
                    disconnectionToken->wait();
                }

                lprintf("INVERTER: All queries complete, exiting loop.");
                exit(0);
            }
            inv->inv_data_avail.store(false);
        }

        sleep(1);
    }

    if (inv)
    {
        inv->terminateThread();
        delete inv;
    }
    return 0;
}
