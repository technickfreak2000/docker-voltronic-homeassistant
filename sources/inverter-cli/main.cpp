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

            mqtt::message_ptr pubMessage = mqtt::make_message("test", "TEST MESSAGE!! TEST MESSAGE!!", QOS, false);
            client->publish(pubMessage)->wait();

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
            char *combined_query = NULL;

            if (qmn->model_name != NULL)
            {
                lprintf("INVERTER: Model name: %s", qmn->model_name);
            }

            if (qmod->inverter_mode_int != 0)
            {
                lprintf("INVERTER: Mode currently set to: %d", qmod->inverter_mode_int);
            }

            cJSON *json = cJSON_CreateObject();

            // Here the json gets build, you can customize the outbut based on the first inverter model name

            // QMN
            cJSON_AddStringToObject(json, "Inverter_model_name", qmn->model_name);

            // QID
            cJSON_AddStringToObject(json, "Inverter_id", qid->inverter_id);

            // QMOD
            cJSON_AddNumberToObject(json, "Inverter_mode", qmod->inverter_mode_int);

            // QFLAG
            cJSON_AddBoolToObject(json, "Silence_open_buzzer", qflag->silence_open_buzzer);
            cJSON_AddBoolToObject(json, "Bypass_function", qflag->bypass_function);
            cJSON_AddBoolToObject(json, "Bypass_function_forbidden", qflag->bypass_function_forbidden);
            cJSON_AddBoolToObject(json, "Power_saving", qflag->power_saving);
            cJSON_AddBoolToObject(json, "Lcd_timeout_default_page", qflag->lcd_timeout_default_page);
            cJSON_AddBoolToObject(json, "Overload_restart", qflag->overload_restart);
            cJSON_AddBoolToObject(json, "Overtemperature_restart", qflag->overtemperature_restart);
            cJSON_AddBoolToObject(json, "Lcd_backlight", qflag->lcd_backlight);
            cJSON_AddBoolToObject(json, "Alarm_primary_input", qflag->alarm_primary_input);
            cJSON_AddBoolToObject(json, "Fault_code_record", qflag->fault_code_record);

            // QVFWn
            QVFWn *current_qvfwn = qvfwn;
            combined_query = (char *)malloc(20 * sizeof(char));
            while (current_qvfwn != NULL)
            {
                sprintf(combined_query, "Fw_version_%d", counter);
                cJSON_AddStringToObject(json, combined_query, current_qvfwn->fw_version);

                counter++;
                current_qvfwn = current_qvfwn->next;
            }
            free(combined_query);

            // QPIGS
            QPIGSn *current_qpigsn = qpigsn;
            counter = 0;
            combined_query = (char *)malloc(50 * sizeof(char));
            while (current_qpigsn != NULL)
            {
                sprintf(combined_query, "SCC_%d_AC_grid_frequency", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpigsn->freq_grid);
                sprintf(combined_query, "SCC_%d_AC_out_voltage", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpigsn->voltage_out);
                sprintf(combined_query, "SCC_%d_AC_out_frequency", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpigsn->freq_out);
                sprintf(combined_query, "SCC_%d_PV_in_voltage", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpigsn->pv_input_voltage);
                sprintf(combined_query, "SCC_%d_PV_in_current", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpigsn->pv_input_current);
                sprintf(combined_query, "SCC_%d_PV_in_watts", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpigsn->pv_input_watts);
                // sprintf(combined_query, "SCC_%d_PV_in_watthour", counter);
                // cJSON_AddNumberToObject(json, combined_query, current_qpigsn->pv_input_watthour);
                sprintf(combined_query, "SCC_%d_SCC_voltage", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpigsn->scc_voltage);
                sprintf(combined_query, "SCC_%d_Load_pct", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpigsn->load_percent);
                sprintf(combined_query, "SCC_%d_Load_watt", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpigsn->load_watt);
                sprintf(combined_query, "SCC_%d_Load_va", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpigsn->load_va);
                // sprintf(combined_query, "SCC_%d_Load_watthour", counter);
                // cJSON_AddNumberToObject(json, combined_query, current_qpigsn->load_watthour);
                sprintf(combined_query, "SCC_%d_Bus_voltage", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpigsn->voltage_bus);
                sprintf(combined_query, "SCC_%d_Heatsink_temperature", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpigsn->temp_heatsink);
                sprintf(combined_query, "SCC_%d_Battery_capacity", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpigsn->batt_capacity);
                sprintf(combined_query, "SCC_%d_Battery_voltage", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpigsn->voltage_batt);
                sprintf(combined_query, "SCC_%d_Battery_charge_current", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpigsn->batt_charge_current);
                sprintf(combined_query, "SCC_%d_Battery_discharge_current", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpigsn->batt_discharge_current);
                sprintf(combined_query, "SCC_%d_Load_status_on", counter);
                cJSON_AddBoolToObject(json, combined_query, current_qpigsn->load_status);
                sprintf(combined_query, "SCC_%d_SCC_charge_on", counter);
                cJSON_AddBoolToObject(json, combined_query, current_qpigsn->charging_status_scc);
                sprintf(combined_query, "SCC_%d_AC_charge_on", counter);
                cJSON_AddBoolToObject(json, combined_query, current_qpigsn->charging_status_ac);
                sprintf(combined_query, "SCC_%d_Battery_voltage_offset_for_fans_on", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpigsn->battery_voltage_offset_for_fans_on);
                sprintf(combined_query, "SCC_%d_Eeprom_version", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpigsn->eeprom_version);
                sprintf(combined_query, "SCC_%d_PV_charging_power", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpigsn->pv_charging_power);
                sprintf(combined_query, "SCC_%d_Charging_to_floating_mode", counter);
                cJSON_AddBoolToObject(json, combined_query, current_qpigsn->charging_to_floating_mode);
                sprintf(combined_query, "SCC_%d_Switch_On", counter);
                cJSON_AddBoolToObject(json, combined_query, current_qpigsn->switch_on);
                sprintf(combined_query, "SCC_%d_Dustproof_installed", counter);
                cJSON_AddBoolToObject(json, combined_query, current_qpigsn->dustproof_installed);
                counter++;
                current_qpigsn = current_qpigsn->next;
            }
            free(combined_query);

            // QPGS
            QPGSn *current_qpgsn = qpgsn;
            counter = 0;
            combined_query = (char *)malloc(50 * sizeof(char));
            while (current_qpgsn != NULL)
            {
                sprintf(combined_query, "INV_%d_Inverter_ID", counter);
                cJSON_AddStringToObject(json, combined_query, current_qpgsn->inverter_id);
                sprintf(combined_query, "INV_%d_Inverter_Mode", counter);
                cJSON_AddStringToObject(json, combined_query, current_qpgsn->inverter_mode);
                sprintf(combined_query, "INV_%d_Inverter_Mode_Int", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->inverter_mode_int);
                sprintf(combined_query, "INV_%d_Fault_Code", counter);
                cJSON_AddStringToObject(json, combined_query, current_qpgsn->fault_code);
                sprintf(combined_query, "INV_%d_Voltage_Grid", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->voltage_grid);
                sprintf(combined_query, "INV_%d_Freq_Grid", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->freq_grid);
                sprintf(combined_query, "INV_%d_Voltage_Out", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->voltage_out);
                sprintf(combined_query, "INV_%d_Freq_Out", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->freq_out);
                sprintf(combined_query, "INV_%d_Load_VA", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->load_va);
                sprintf(combined_query, "INV_%d_Load_Watt", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->load_watt);
                sprintf(combined_query, "INV_%d_Load_Percent", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->load_percent);
                sprintf(combined_query, "INV_%d_Voltage_Batt", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->voltage_batt);
                sprintf(combined_query, "INV_%d_Batt_Charge_Current", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->batt_charge_current);
                sprintf(combined_query, "INV_%d_Batt_Capacity", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->batt_capacity);
                sprintf(combined_query, "INV_%d_PV_Input_Voltage", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->pv_input_voltage);
                sprintf(combined_query, "INV_%d_Batt_Charge_Current_Total", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->batt_charge_current_total);
                sprintf(combined_query, "INV_%d_Load_VA_Total", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->load_va_total);
                sprintf(combined_query, "INV_%d_Load_Watt_Total", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->load_watt_total);
                sprintf(combined_query, "INV_%d_Load_Percent_Total", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->load_percent_total);
                sprintf(combined_query, "INV_%d_SCC_OK", counter);
                cJSON_AddBoolToObject(json, combined_query, atomic_load(&current_qpgsn->scc_ok));
                sprintf(combined_query, "INV_%d_Charging_Status_AC", counter);
                cJSON_AddBoolToObject(json, combined_query, atomic_load(&current_qpgsn->charging_status_ac));
                sprintf(combined_query, "INV_%d_Charging_Status_SCC", counter);
                cJSON_AddBoolToObject(json, combined_query, atomic_load(&current_qpgsn->charging_status_scc));
                sprintf(combined_query, "INV_%d_Battery_Open", counter);
                cJSON_AddBoolToObject(json, combined_query, atomic_load(&current_qpgsn->battery_open));
                sprintf(combined_query, "INV_%d_Battery_Under", counter);
                cJSON_AddBoolToObject(json, combined_query, atomic_load(&current_qpgsn->battery_under));
                sprintf(combined_query, "INV_%d_Battery_Normal", counter);
                cJSON_AddBoolToObject(json, combined_query, atomic_load(&current_qpgsn->battery_normal));
                sprintf(combined_query, "INV_%d_AC_Loss", counter);
                cJSON_AddBoolToObject(json, combined_query, atomic_load(&current_qpgsn->ac_loss));
                sprintf(combined_query, "INV_%d_AC_OK", counter);
                cJSON_AddBoolToObject(json, combined_query, atomic_load(&current_qpgsn->ac_ok));
                sprintf(combined_query, "INV_%d_Configuration_Changed", counter);
                cJSON_AddBoolToObject(json, combined_query, atomic_load(&current_qpgsn->configuration_changed));
                sprintf(combined_query, "INV_%d_Output_Mode", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->output_mode);
                sprintf(combined_query, "INV_%d_Charger_Source_Priority", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->charger_source_priority);
                sprintf(combined_query, "INV_%d_Max_Charger_Current", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->max_charger_current);
                sprintf(combined_query, "INV_%d_Max_Charger_Range", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->max_charger_range);
                sprintf(combined_query, "INV_%d_Max_AC_Charger_Current", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->max_ac_charger_current);
                sprintf(combined_query, "INV_%d_PV_Input_Current", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->pv_input_current);
                sprintf(combined_query, "INV_%d_Batt_Discharge_Current", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->batt_discharge_current);
                sprintf(combined_query, "INV_%d_PV_Input_Watts", counter);
                cJSON_AddNumberToObject(json, combined_query, current_qpgsn->pv_input_watts);
                counter++;
                current_qpgsn = current_qpgsn->next;
            }
            free(combined_query);

            // QPIRI
            cJSON_AddNumberToObject(json, "Battery_recharge_voltage", qpiri->batt_recharge_voltage);
            cJSON_AddNumberToObject(json, "Battery_under_voltage", qpiri->batt_under_voltage);
            cJSON_AddNumberToObject(json, "Battery_bulk_voltage", qpiri->batt_bulk_voltage);
            cJSON_AddNumberToObject(json, "Battery_float_voltage", qpiri->batt_float_voltage);
            cJSON_AddNumberToObject(json, "Max_grid_charge_current", qpiri->max_grid_charge_current);
            cJSON_AddNumberToObject(json, "Max_charge_current", qpiri->max_charge_current);
            cJSON_AddNumberToObject(json, "Out_source_priority", qpiri->out_source_priority);
            cJSON_AddNumberToObject(json, "Charger_source_priority", qpiri->charger_source_priority);
            cJSON_AddNumberToObject(json, "Battery_redischarge_voltage", qpiri->batt_redischarge_voltage);

            // QPIWS
            cJSON_AddBoolToObject(json, "Reserved", qpiws->reserved);
            cJSON_AddBoolToObject(json, "Inverter_fault", qpiws->inverter_fault);
            cJSON_AddBoolToObject(json, "Bus_over", qpiws->bus_over);
            cJSON_AddBoolToObject(json, "Bus_under", qpiws->bus_under);
            cJSON_AddBoolToObject(json, "Bus_soft_fail", qpiws->bus_soft_fail);
            cJSON_AddBoolToObject(json, "Line_fail", qpiws->line_fail);
            cJSON_AddBoolToObject(json, "OPV_short", qpiws->opv_short);
            cJSON_AddBoolToObject(json, "Inverter_voltage_too_low", qpiws->inverter_voltage_too_low);
            cJSON_AddBoolToObject(json, "Inverter_voltage_too_high", qpiws->inverter_voltage_too_high);
            cJSON_AddBoolToObject(json, "Over_temperature", qpiws->over_temperature);
            cJSON_AddBoolToObject(json, "Fan_locked", qpiws->fan_locked);
            cJSON_AddBoolToObject(json, "Battery_voltage_high", qpiws->battery_voltage_high);
            cJSON_AddBoolToObject(json, "Battery_low_alarm", qpiws->battery_low_alarm);
            cJSON_AddBoolToObject(json, "Overcharge", qpiws->overcharge);
            cJSON_AddBoolToObject(json, "Battery_under_shutdown", qpiws->battery_under_shutdown);
            cJSON_AddBoolToObject(json, "Battery_derating", qpiws->battery_derating);
            cJSON_AddBoolToObject(json, "Over_load", qpiws->over_load);
            cJSON_AddBoolToObject(json, "EEPROM_fault", qpiws->eeprom_fault);
            cJSON_AddBoolToObject(json, "Inverter_over_current", qpiws->inverter_over_current);
            cJSON_AddBoolToObject(json, "Inverter_soft_fail", qpiws->inverter_soft_fail);
            cJSON_AddBoolToObject(json, "Self_test_fail", qpiws->self_test_fail);
            cJSON_AddBoolToObject(json, "OP_DC_voltage_over", qpiws->op_dc_voltage_over);
            cJSON_AddBoolToObject(json, "BAT_open", qpiws->bat_open);
            cJSON_AddBoolToObject(json, "Current_sensor_fail", qpiws->current_sensor_fail);
            cJSON_AddBoolToObject(json, "Battery_short", qpiws->battery_short);
            cJSON_AddBoolToObject(json, "Power_limit", qpiws->power_limit);
            cJSON_AddBoolToObject(json, "PV_voltage_high", qpiws->pv_voltage_high);
            cJSON_AddBoolToObject(json, "MPPT_overload_fault", qpiws->mppt_overload_fault);
            cJSON_AddBoolToObject(json, "MPPT_overload_warning", qpiws->mppt_overload_warning);
            cJSON_AddBoolToObject(json, "Battery_too_low_to_charge", qpiws->battery_too_low_to_charge);
            cJSON_AddBoolToObject(json, "DC_DC_over_current", qpiws->dc_dc_over_current);
            cJSON_AddBoolToObject(json, "Reserved2", qpiws->reserved2);

            char *jsonString = cJSON_Print(json);

            if (mqtt_sel)
            {
                // todo iterate through cjson and push mqtt
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
                    std::string topic = std::string(config_mqtt.topic) + "/sensor/" + std::string(config_mqtt.device_name) + "/" + std::string(key);
                    mqtt::message_ptr pubMessage = mqtt::make_message(topic, std::string(value_str), QOS, false);
                    client->publish(pubMessage)->wait();
                }
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
