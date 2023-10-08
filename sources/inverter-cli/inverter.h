#ifndef ___INVERTER_H
#define ___INVERTER_H

#define bufsize 1024

#include <atomic>
#include <thread>
#include <mutex>

using namespace std;

// Declaration of implemented read commands
typedef struct QMOD // Modus of first inverter
{
  char inverter_mode[2];
  int inverter_mode_int;
} QMOD;

typedef struct QPIGSn // Standard charging, mppt, etc information of all controllers of a single Inverter
{
  double voltage_grid = 0;
  double freq_grid = 0;
  double voltage_out = 0;
  double freq_out = 0;
  int load_va = 0;
  int load_watt = 0;
  int load_percent = 0;
  int voltage_bus = 0;
  double voltage_batt = 0;
  int batt_charge_current = 0;
  int batt_capacity = 0;
  int temp_heatsink = 0;
  double pv_input_current = 0;
  double pv_input_voltage = 0;
  double pv_input_watts = 0;
  double scc_voltage = 0;
  int batt_discharge_current = 0;
  atomic_bool add_sbu_priority_version = ATOMIC_VAR_INIT(false);                 // b7
  atomic_bool configuration_status_change = ATOMIC_VAR_INIT(false);              // b6
  atomic_bool scc_firmware_version_change = ATOMIC_VAR_INIT(false);              // b5
  atomic_bool load_status = ATOMIC_VAR_INIT(false);                              // b4
  atomic_bool battery_voltage_to_steady_while_charging = ATOMIC_VAR_INIT(false); // b3
  atomic_bool charging_status_charging = ATOMIC_VAR_INIT(false);                 // b2
  atomic_bool charging_status_scc = ATOMIC_VAR_INIT(false);                      // b1
  atomic_bool charging_status_ac = ATOMIC_VAR_INIT(false);                       // b0
  int battery_voltage_offset_for_fans_on = 0;
  int eeprom_version = 0;
  int pv_charging_power = 0;
  atomic_bool charging_to_floating_mode = ATOMIC_VAR_INIT(false); // b10
  atomic_bool switch_on = ATOMIC_VAR_INIT(false);                 // b9
  atomic_bool dustproof_installed = ATOMIC_VAR_INIT(false);       // b8   Axpert V series only?

  struct QPIGSn *next = NULL;
} QPIGSn;

typedef struct QPGSn // All kinds of information + QPIGS of all inverter
{
  atomic_bool parallel_num_exists = ATOMIC_VAR_INIT(false);
  char inverter_id[15];
  char inverter_mode[2];
  int inverter_mode_int;
  char fault_code[3];
  double voltage_grid = 0;
  double freq_grid = 0;
  double voltage_out = 0;
  double freq_out = 0;
  int load_va = 0;
  int load_watt = 0;
  int load_percent = 0;
  double voltage_batt = 0;
  int batt_charge_current = 0;
  int batt_capacity = 0;
  double pv_input_voltage = 0;
  int batt_charge_current_total = 0;
  int load_va_total = 0;
  int load_watt_total = 0;
  int load_percent_total = 0;
  atomic_bool scc_ok = ATOMIC_VAR_INIT(false);                // b7
  atomic_bool charging_status_ac = ATOMIC_VAR_INIT(false);    // b6
  atomic_bool charging_status_scc = ATOMIC_VAR_INIT(false);   // b5
  atomic_bool battery_open = ATOMIC_VAR_INIT(false);          // b4 == 1 && b3 == 0
  atomic_bool battery_under = ATOMIC_VAR_INIT(false);         // b4 == 0 && b3 == 1
  atomic_bool battery_normal = ATOMIC_VAR_INIT(false);        // b4 == 0 && b3 == 0
  atomic_bool ac_loss = ATOMIC_VAR_INIT(false);               // b2
  atomic_bool ac_ok = ATOMIC_VAR_INIT(false);                 // b1
  atomic_bool configuration_changed = ATOMIC_VAR_INIT(false); // b0
  int output_mode = 0;
  int charger_source_priority = 0;
  int max_charger_current = 0;
  int max_charger_range = 0;
  int max_ac_charger_current = 0;
  double pv_input_current = 0;
  int batt_discharge_current = 0;
  double pv_input_watts = 0;

  struct QPGSn *next = NULL;
} QPGSn;

typedef struct QPIRI // "global" values of inverter for battery settings, ...
{
  double grid_voltage_rating = 0;
  double grid_current_rating = 0;
  double out_voltage_rating = 0;
  double out_freq_rating = 0;
  double out_current_rating = 0;
  int out_va_rating = 0;
  int out_watt_rating = 0;
  double batt_rating = 0;
  double batt_recharge_voltage = 0;
  double batt_under_voltage = 0;
  double batt_bulk_voltage = 0;
  double batt_float_voltage = 0;
  int batt_type = 0;
  int max_grid_charge_current = 0;
  int max_charge_current = 0;
  int in_voltage_range = 0;
  int out_source_priority = 0;
  int charger_source_priority = 0;
  int machine_type = 0;
  int topology = 0;
  int out_mode = 0;
  double batt_redischarge_voltage = 0;
  char parallel_max_num = 0;
} QPIRI;

typedef struct QPIWS // Warning bits of first inverter
{
  // warning status a0-a31
  atomic_bool reserved = ATOMIC_VAR_INIT(false);                  // a0
  atomic_bool inverter_fault = ATOMIC_VAR_INIT(false);            // a1
  atomic_bool bus_over = ATOMIC_VAR_INIT(false);                  // a2
  atomic_bool bus_under = ATOMIC_VAR_INIT(false);                 // a3
  atomic_bool bus_soft_fail = ATOMIC_VAR_INIT(false);             // a4
  atomic_bool line_fail = ATOMIC_VAR_INIT(false);                 // a5
  atomic_bool opv_short = ATOMIC_VAR_INIT(false);                 // a6
  atomic_bool inverter_voltage_too_low = ATOMIC_VAR_INIT(false);  // a7
  atomic_bool inverter_voltage_too_high = ATOMIC_VAR_INIT(false); // a8
  atomic_bool over_temperature = ATOMIC_VAR_INIT(false);          // a9
  atomic_bool fan_locked = ATOMIC_VAR_INIT(false);                // a10
  atomic_bool battery_voltage_high = ATOMIC_VAR_INIT(false);      // a11
  atomic_bool battery_low_alarm = ATOMIC_VAR_INIT(false);         // a12
  atomic_bool overcharge = ATOMIC_VAR_INIT(false);                // a13
  atomic_bool battery_under_shutdown = ATOMIC_VAR_INIT(false);    // a14
  atomic_bool battery_derating = ATOMIC_VAR_INIT(false);          // a15
  atomic_bool over_load = ATOMIC_VAR_INIT(false);                 // a16
  atomic_bool eeprom_fault = ATOMIC_VAR_INIT(false);              // a17
  atomic_bool inverter_over_current = ATOMIC_VAR_INIT(false);     // a18
  atomic_bool inverter_soft_fail = ATOMIC_VAR_INIT(false);        // a19
  atomic_bool self_test_fail = ATOMIC_VAR_INIT(false);            // a20
  atomic_bool op_dc_voltage_over = ATOMIC_VAR_INIT(false);        // a21
  atomic_bool bat_open = ATOMIC_VAR_INIT(false);                  // a22
  atomic_bool current_sensor_fail = ATOMIC_VAR_INIT(false);       // a23
  atomic_bool battery_short = ATOMIC_VAR_INIT(false);             // a24
  atomic_bool power_limit = ATOMIC_VAR_INIT(false);               // a25
  atomic_bool pv_voltage_high = ATOMIC_VAR_INIT(false);           // a26
  atomic_bool mppt_overload_fault = ATOMIC_VAR_INIT(false);       // a27
  atomic_bool mppt_overload_warning = ATOMIC_VAR_INIT(false);     // a28
  atomic_bool battery_too_low_to_charge = ATOMIC_VAR_INIT(false); // a29
  atomic_bool dc_dc_over_current = ATOMIC_VAR_INIT(false);        // a30
  atomic_bool reserved2 = ATOMIC_VAR_INIT(false);                 // a31

  // Seems to be for king only?
  char fault_code[3]; // a32-33
} QPIWS;

typedef struct QVFWn // Firmware version of all processors inside first inverter
{
  char *fw_version = NULL;

  struct QVFWn *next = NULL;
} QVFWn;

typedef struct QMN // Model name
{
  char *model_name = NULL;
} QMN;

typedef struct QFLAG // Inverter capabilities
{
  atomic_bool silence_open_buzzer = ATOMIC_VAR_INIT(false);       // Enable/disable silence or open buzzer
  atomic_bool bypass_function = ATOMIC_VAR_INIT(false);           // Enable/disable bypass function
  atomic_bool bypass_function_forbidden = ATOMIC_VAR_INIT(false); // Enable/forbid bypass function
  atomic_bool power_saving = ATOMIC_VAR_INIT(false);              // Enable/disable power saving
  atomic_bool lcd_timeout_default_page = ATOMIC_VAR_INIT(false);  // Enable/disable LCD display escape to default page after 1min timeout
  atomic_bool overload_restart = ATOMIC_VAR_INIT(false);          // Enable/disable overload restart
  atomic_bool overtemperature_restart = ATOMIC_VAR_INIT(false);   // Enable/disable over temperature restart
  atomic_bool lcd_backlight = ATOMIC_VAR_INIT(false);             // Enable/disable backlight
  atomic_bool alarm_primary_input = ATOMIC_VAR_INIT(false);       // Enable/disable alarm when primary input interrupts
  atomic_bool fault_code_record = ATOMIC_VAR_INIT(false);         // Enable/disable fault code record
} QFLAG;

typedef struct QID // ID of inverter
{
  char *inverter_id = NULL;
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
  bool runOnce = false;
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

  QMN qmn;
  QID qid;
  QFLAG qflag;
  QVFWn qvfwn;
  QMOD qmod;
  QPIGSn qpigsn;
  QPGSn qpgsn;
  QPIRI qpiri;
  QPIWS qpiws;
  atomic_bool inv_data_avail = ATOMIC_VAR_INIT(false);

  string *ExecuteCmd(const std::string cmd);
};

#endif // ___INVERTER_H