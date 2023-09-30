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

bool debugFlag = false;
bool runOnce = false;
cInverter *inv = NULL;

// ---------------------------------------
// Global configs read from 'inverter.conf'

string devicename;
float ampfactor;
float wattfactor;

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
                    devicename = linepart2;
                // else if(linepart1 == "run_interval")
                //     attemptAddSetting(&runinterval, linepart2);
                else if (linepart1 == "amperage_factor")
                    attemptAddSetting(&ampfactor, linepart2);
                else if (linepart1 == "watt_factor")
                    attemptAddSetting(&wattfactor, linepart2);
                else
                    continue;
            }
        }
        infile.close();
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

    inv = new cInverter(devicename);
    // Logic to send 'raw commands' to the inverter..
    if (!rawcmd.empty())
    {
        // Execute and return
        printf("Reply:  %s\n", inv->ExecuteCmd(rawcmd)->c_str());
        exit(0);
    }

    if (runOnce)
        inv->poll();
    else
        inv->runMultiThread();

    while (true)
    {
        lprintf("DEBUG:  Start loop");

        // Once we receive all queries print it to screen
        if (inv->inv_data_avail)
        {
            QMN *qmn = &inv->qmn;
            QID *qid = &inv->qid;
            QMOD *qmod = &inv->qmod;
            QPIGSn *qpigsn = &inv->qpigsn;
            QPIRI *qpiri = &inv->qpiri;
            QPIWS *qpiws = &inv->qpiws;

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

            // QPIGS
            cJSON_AddNumberToObject(json, "AC_grid_voltage", qpigsn->voltage_grid);
            cJSON_AddNumberToObject(json, "AC_grid_frequency", qpigsn->freq_grid);
            cJSON_AddNumberToObject(json, "AC_out_voltage", qpigsn->voltage_out);
            cJSON_AddNumberToObject(json, "AC_out_frequency", qpigsn->freq_out);
            cJSON_AddNumberToObject(json, "PV_in_voltage", qpigsn->pv_input_voltage);
            cJSON_AddNumberToObject(json, "PV_in_current", qpigsn->pv_input_current); // This seems to be only the PV charging input current, NOT THE ACTUAL PV CURRENT ITSELF!!! At the moment, I havn't found a way to get the actual input current!
            cJSON_AddNumberToObject(json, "PV_in_watts", qpigsn->pv_input_watts);     // This seems to be only the PV charging input current * voltage, NOT THE ACTUAL PV WATTAGE ITSELF!!!
            // cJSON_AddNumberToObject(json, "PV_in_watthour", qpigsn->pv_input_watthour);
            cJSON_AddNumberToObject(json, "SCC_voltage", qpigsn->scc_voltage);
            cJSON_AddNumberToObject(json, "Load_pct", qpigsn->load_percent);
            cJSON_AddNumberToObject(json, "Load_watt", qpigsn->load_watt);
            // cJSON_AddNumberToObject(json, "Load_watthour", qpigsn->load_watthour);
            cJSON_AddNumberToObject(json, "Load_va", qpigsn->load_va);
            cJSON_AddNumberToObject(json, "Bus_voltage", qpigsn->voltage_bus);
            cJSON_AddNumberToObject(json, "Heatsink_temperature", qpigsn->temp_heatsink);
            cJSON_AddNumberToObject(json, "Battery_capacity", qpigsn->batt_capacity);
            cJSON_AddNumberToObject(json, "Battery_voltage", qpigsn->voltage_batt);
            cJSON_AddNumberToObject(json, "Battery_charge_current", qpigsn->batt_charge_current);
            cJSON_AddNumberToObject(json, "Battery_discharge_current", qpigsn->batt_discharge_current);
            cJSON_AddBoolToObject(json, "Load_status_on", qpigsn->load_status);
            cJSON_AddBoolToObject(json, "SCC_charge_on", qpigsn->charging_status_scc);
            cJSON_AddBoolToObject(json, "AC_charge_on", qpigsn->charging_status_ac);
            cJSON_AddNumberToObject(json, "Battery_voltage_offset_for_fans_on", qpigsn->battery_voltage_offset_for_fans_on);
            cJSON_AddNumberToObject(json, "Eeprom_version", qpigsn->eeprom_version);
            cJSON_AddNumberToObject(json, "PV_charging_power", qpigsn->pv_charging_power);
            cJSON_AddBoolToObject(json, "Charging_to_floating_mode", qpigsn->charging_to_floating_mode);
            cJSON_AddBoolToObject(json, "Switch_On", qpigsn->switch_on);
            cJSON_AddBoolToObject(json, "Dustproof_installed", qpigsn->dustproof_installed);

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

            char *jsonString = cJSON_Print(json);

            printf("%s\n", jsonString);

            cJSON_free(jsonString);
            cJSON_Delete(json);

            if (runOnce)
            {
                // there is no thread -- ups->terminateThread();
                // Do once and exit instead of loop endlessly
                lprintf("INVERTER: All queries complete, exiting loop.");
                exit(0);
            }
            inv->inv_data_avail = false;
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
