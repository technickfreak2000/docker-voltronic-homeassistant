#ifndef ___INVERTER_H
#define ___INVERTER_H

#define bufsize 1024

#include <atomic>
#include <thread>
#include <mutex>

using namespace std;

// Declaration of implemented read commands
typedef struct // Modus of first inverter
{
  char inverter_mode[2];
  int inverter_mode_int;
} QMOD;

typedef struct QPIGSn // Standard charging, mppt, etc information of all controllers of a single Inverter
{
  float voltage_grid;
  float freq_grid;
  float voltage_out;
  float freq_out;
  int load_va;
  int load_watt;
  int load_percent;
  int voltage_bus;
  float voltage_batt;
  int batt_charge_current;
  int batt_capacity;
  int temp_heatsink;
  float pv_input_current;
  float pv_input_voltage;
  float pv_input_watts;
  float scc_voltage;
  int batt_discharge_current;
  atomic_bool add_sbu_priority_version;                 // b7
  atomic_bool configuration_status_change;              // b6
  atomic_bool scc_firmware_version_change;              // b5
  atomic_bool load_status;                              // b4
  atomic_bool battery_voltage_to_steady_while_charging; // b3
  atomic_bool charging_status_charging;                 // b2
  atomic_bool charging_status_scc;                      // b1
  atomic_bool charging_status_ac;                       // b0
  int battery_voltage_offset_for_fans_on;
  int eeprom_version;
  int pv_charging_power;
  atomic_bool charging_to_floating_mode; // b10
  atomic_bool switch_on;                 // b9
  atomic_bool dustproof_installed;       // b8   Axpert V series only?

  struct QPIGSn *next;
} QPIGSn;

typedef struct QPGSn // All kinds of information + QPIGS of all inverter
{

  struct QPGSn *next;
} QPGSn;

typedef struct QPIRI // "global" values of inverter for battery settings, ...
{
  float grid_voltage_rating;
  float grid_current_rating;
  float out_voltage_rating;
  float out_freq_rating;
  float out_current_rating;
  int out_va_rating;
  int out_watt_rating;
  float batt_rating;
  float batt_recharge_voltage;
  float batt_under_voltage;
  float batt_bulk_voltage;
  float batt_float_voltage;
  int batt_type;
  int max_grid_charge_current;
  int max_charge_current;
  int in_voltage_range;
  int out_source_priority;
  int charger_source_priority;
  int machine_type;
  int topology;
  int out_mode;
  float batt_redischarge_voltage;
  char parallel_max_num;
} QPIRI;

typedef struct QPIWS // Warning bits of first inverter
{
  // warning status a0-a31
  atomic_bool reserved;                  // a0
  atomic_bool inverter_fault;            // a1
  atomic_bool bus_over;                  // a2
  atomic_bool bus_under;                 // a3
  atomic_bool bus_soft_fail;             // a4
  atomic_bool line_fail;                 // a5
  atomic_bool opv_short;                 // a6
  atomic_bool inverter_voltage_too_low;  // a7
  atomic_bool inverter_voltage_too_high; // a8
  atomic_bool over_temperature;          // a9
  atomic_bool fan_locked;                // a10
  atomic_bool battery_voltage_high;      // a11
  atomic_bool battery_low_alarm;         // a12
  atomic_bool overcharge;                // a13
  atomic_bool battery_under_shutdown;    // a14
  atomic_bool battery_derating;          // a15
  atomic_bool over_load;                 // a16
  atomic_bool eeprom_fault;              // a17
  atomic_bool inverter_over_current;     // a18
  atomic_bool inverter_soft_fail;        // a19
  atomic_bool self_test_fail;            // a20
  atomic_bool op_dc_voltage_over;        // a21
  atomic_bool bat_open;                  // a22
  atomic_bool current_sensor_fail;       // a23
  atomic_bool battery_short;             // a24
  atomic_bool power_limit;               // a25
  atomic_bool pv_voltage_high;           // a26
  atomic_bool mppt_overload_fault;       // a27
  atomic_bool mppt_overload_warning;     // a28
  atomic_bool battery_too_low_to_charge; // a29
  atomic_bool dc_dc_over_current;        // a30
  atomic_bool reserved2;                 // a31

  // Seems to be for king only?
  char fault_code[3]; // a32-33
} QPIWS;

typedef struct QVFWn // Firmware version of all processors inside first inverter
{

  struct QVFWn *next;
} QVFWn;

typedef struct QMN // Model name
{
  string model_name;
} QMN;

typedef struct QFLAG // Inverter capabilities
{

} QFLAG;

typedef struct QID // ID of inverter
{

} QID;

class cInverter
{
  // Internal work buffer
  unsigned char buf[bufsize];

  std::string device;
  std::mutex m;
  std::thread t1;
  std::atomic_bool quit_thread{false};

  void SetMode(char newmode);
  bool CheckCRC(unsigned char *buff, int len);
  bool query(const char *cmd);
  uint16_t cal_crc_half(uint8_t *pin, uint8_t len);

  // Declaration of implemented read functions
  void GetQMOD(QMOD *qmod);
  void GetQPIGSn(QPIGSn *qpigsn);
  void GetQPGSn(QPGSn *qpgsn);
  void GetQPIRI(QPIRI *qpiri);
  void GetQPIWS(QPIWS *qpiws);
  void GetQVWFn(QVFWn *qvwfn);
  void GetQMN(QMN *qmn);
  void GetQFLAG(QFLAG *qflag);
  void GetQID(QID *qid);

public:
  cInverter(std::string devicename);
  void poll();
  void runMultiThread()
  {
    t1 = std::thread(&cInverter::poll, this);
  }
  void terminateThread()
  {
    quit_thread = true;
    t1.join();
  }

  QMOD qmod;
  QPIGSn qpigsn;
  QPIRI qpiri;
  QPIWS qpiws;
  atomic_bool inv_data_avail;

  string *ExecuteCmd(const std::string cmd);
};

#endif // ___INVERTER_H