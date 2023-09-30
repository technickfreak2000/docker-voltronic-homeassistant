#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include "inverter.h"
#include "tools.h"
#include "main.h"

#include <termios.h>

cInverter::cInverter(std::string devicename)
{
  device = devicename;
}

void cInverter::GetQMOD(QMOD *qmod)
{
  if (query("QMOD") &&
      strcmp((char *)&buf[1], "NAK") != 0)
  {
    m.lock();
    qmod->inverter_mode[0] = buf[1];
    qmod->inverter_mode[1] = '\0';

    switch (qmod->inverter_mode[0])
    {
    case 'P':
      qmod->inverter_mode_int = 1;
      break; // Power_On
    case 'S':
      qmod->inverter_mode_int = 2;
      break; // Standby
    case 'L':
      qmod->inverter_mode_int = 3;
      break; // Line
    case 'B':
      qmod->inverter_mode_int = 4;
      break; // Battery
    case 'F':
      qmod->inverter_mode_int = 5;
      break; // Fault
    case 'H':
      qmod->inverter_mode_int = 6;
      break; // Power_Saving
    default:
      qmod->inverter_mode_int = 0;
      break; // Unknown
    }
    m.unlock();
  }
}

void cInverter::GetQPIGSn(QPIGSn *qpigsn)
{
  if (query("QPIGS") &&
      strcmp((char *)&buf[1], "NAK") != 0)
  {
    m.lock();
    string *tmpData = new string((const char *)buf + 1);
    char device_status[8];
    char device_status2[3];

    // Parse values
    sscanf(tmpData->c_str(), "%lf %lf %lf %lf %d %d %d %d %lf %d %d %d %lf %lf %lf %d %s %d %d %d %s",
           &qpigsn->voltage_grid,                       // Grid voltage
           &qpigsn->freq_grid,                          // Grid frequency
           &qpigsn->voltage_out,                        // AC output voltage
           &qpigsn->freq_out,                           // AC output frequency
           &qpigsn->load_va,                            // AC output apparent power (VA)
           &qpigsn->load_watt,                          // AC output active power (Watt)
           &qpigsn->load_percent,                       // Output load percent - Maximum of W% or VA%., VA% is a percent of apparent power., W% is a percent of active power.
           &qpigsn->voltage_bus,                        // BUS voltage
           &qpigsn->voltage_batt,                       // Battery voltage
           &qpigsn->batt_charge_current,                // Battery charging current
           &qpigsn->batt_capacity,                      // Battery capacity
           &qpigsn->temp_heatsink,                      // Inverter heat sink temperature
           &qpigsn->pv_input_current,                   // PV Input current for battery.
           &qpigsn->pv_input_voltage,                   // PV Input voltage 1
           &qpigsn->scc_voltage,                        // Battery voltage from SCC (Solar Charge Controller) (V)
           &qpigsn->batt_discharge_current,             // Battery discharge current
           device_status,                               // Device Status bits
           &qpigsn->battery_voltage_offset_for_fans_on, // Battery voltage offset for fans
           &qpigsn->eeprom_version,                     // EEPROM version
           &qpigsn->pv_charging_power,                  // PV charging power (W)
           device_status2                               // Device status bits 2
    );

    // Parse through device status bits
    qpigsn->add_sbu_priority_version.store(device_status[0] == '1');
    qpigsn->configuration_status_change.store(device_status[1] == '1');
    qpigsn->scc_firmware_version_change.store(device_status[2] == '1');
    qpigsn->load_status.store(device_status[3] == '1');
    qpigsn->battery_voltage_to_steady_while_charging.store(device_status[4] == '1');
    qpigsn->charging_status_charging.store(device_status[5] == '1');
    qpigsn->charging_status_scc.store(device_status[6] == '1');
    qpigsn->charging_status_ac.store(device_status[7] == '1');

    // Parse through other device status bits
    qpigsn->charging_to_floating_mode.store(device_status2[0] == '1');
    qpigsn->switch_on.store(device_status2[1] == '1');
    qpigsn->dustproof_installed.store(device_status2[2] == '1');

    // Calculate pv input wattage
    qpigsn->pv_input_watts = qpigsn->pv_input_current * qpigsn->pv_input_voltage;

    delete tmpData;
    m.unlock();
  }
}

void cInverter::GetQPGSn(QPGSn *qpgsn)
{
}

void cInverter::GetQPIRI(QPIRI *qpiri)
{
  if (query("QPIRI") &&
      strcmp((char *)&buf[1], "NAK") != 0)
  {
    m.lock();
    string *tmpData = new string((const char *)buf + 1);

    // Parse values
    sscanf(tmpData->c_str(), "%lf %lf %lf %lf %lf %d %d %lf %lf %lf %lf %lf %d %d %d %d %d %d %c %d %d %d %lf",
           &qpiri->grid_voltage_rating,       // ^ Grid rating voltage
           &qpiri->grid_current_rating,       // ^ Grid rating current per protocol, frequency in practice
           &qpiri->out_voltage_rating,        // ^ AC output rating voltage
           &qpiri->out_freq_rating,           // ^ AC output rating frequency
           &qpiri->out_current_rating,        // ^ AC output rating current
           &qpiri->out_va_rating,             // ^ AC output rating apparent power
           &qpiri->out_watt_rating,           // ^ AC output rating active power
           &qpiri->batt_rating,               // ^ Battery rating voltage
           &qpiri->batt_recharge_voltage,     // * Battery re-charge voltage
           &qpiri->batt_under_voltage,        // * Battery under voltage
           &qpiri->batt_bulk_voltage,         // * Battery bulk voltage
           &qpiri->batt_float_voltage,        // * Battery float voltage
           &qpiri->batt_type,                 // ^ Battery type - 0 AGM, 1 Flooded, 2 User
           &qpiri->max_grid_charge_current,   // * Current max AC charging current
           &qpiri->max_charge_current,        // * Current max charging current
           &qpiri->in_voltage_range,          // ^ Input voltage range, 0 Appliance 1 UPS
           &qpiri->out_source_priority,       // * Output source priority, 0 Utility first, 1 solar first, 2 SUB first
           &qpiri->charger_source_priority,   // * Charger source priority 0 Utility first, 1 solar first, 2 Solar + utility, 3 only solar charging permitted
           &qpiri->parallel_max_num,          // ^ Parallel max number 0-9
           &qpiri->machine_type,              // ^ Machine type 00 Grid tie, 01 Off grid, 10 hybrid
           &qpiri->topology,                  // ^ Topology  0 transformerless 1 transformer
           &qpiri->out_mode,                  // ^ Output mode 00: single machine output, 01: parallel output, 02: Phase 1 of 3 Phase output, 03: Phase 2 of 3 Phase output, 04: Phase 3 of 3 Phase output
           &qpiri->batt_redischarge_voltage); // * Battery re-discharge voltage
                                              // ^ PV OK condition for parallel
                                              // ^ PV power balance
    delete tmpData;
    m.unlock();
  }
}

void cInverter::GetQPIWS(QPIWS *qpiws)
{
  if (query("QPIWS") &&
      strcmp((char *)&buf[1], "NAK") != 0)
  {
    m.lock();
    char *tmpData = (char *)buf + 1;

    // Parse warning bits a0-a31
    qpiws->reserved.store(tmpData[0] == '1');
    qpiws->inverter_fault.store(tmpData[1] == '1');
    qpiws->bus_over.store(tmpData[2] == '1');
    qpiws->bus_under.store(tmpData[3] == '1');
    qpiws->bus_soft_fail.store(tmpData[4] == '1');
    qpiws->line_fail.store(tmpData[5] == '1');
    qpiws->opv_short.store(tmpData[6] == '1');
    qpiws->inverter_voltage_too_low.store(tmpData[7] == '1');
    qpiws->inverter_voltage_too_high.store(tmpData[8] == '1');
    qpiws->over_temperature.store(tmpData[9] == '1');
    qpiws->fan_locked.store(tmpData[10] == '1');
    qpiws->battery_voltage_high.store(tmpData[11] == '1');
    qpiws->battery_low_alarm.store(tmpData[12] == '1');
    qpiws->overcharge.store(tmpData[13] == '1');
    qpiws->battery_under_shutdown.store(tmpData[14] == '1');
    qpiws->battery_derating.store(tmpData[15] == '1');
    qpiws->over_load.store(tmpData[16] == '1');
    qpiws->eeprom_fault.store(tmpData[17] == '1');
    qpiws->inverter_over_current.store(tmpData[18] == '1');
    qpiws->inverter_soft_fail.store(tmpData[19] == '1');
    qpiws->self_test_fail.store(tmpData[20] == '1');
    qpiws->op_dc_voltage_over.store(tmpData[21] == '1');
    qpiws->bat_open.store(tmpData[22] == '1');
    qpiws->current_sensor_fail.store(tmpData[23] == '1');
    qpiws->battery_short.store(tmpData[24] == '1');
    qpiws->power_limit.store(tmpData[25] == '1');
    qpiws->pv_voltage_high.store(tmpData[26] == '1');
    qpiws->mppt_overload_fault.store(tmpData[27] == '1');
    qpiws->mppt_overload_warning.store(tmpData[28] == '1');
    qpiws->battery_too_low_to_charge.store(tmpData[29] == '1');
    qpiws->dc_dc_over_current.store(tmpData[30] == '1');

    // Parse fault code (a32-a33)
    strncpy(qpiws->fault_code, tmpData + 32, 2);
    qpiws->fault_code[2] = '\0';

    m.unlock();
  }
}

void cInverter::GetQVWFn(QVFWn *qvwfn)
{
}

void cInverter::GetQMN(QMN *qmn)
{
  if (query("QMN") &&
      strcmp((char *)&buf[1], "NAK") != 0)
  {
    m.lock();
    char *tmpData = (char *)buf + 1;

    if (qmn->model_name != NULL)
    {
      free(qmn->model_name);
    }

    size_t model_name_length = strlen(tmpData) + 1;
    qmn->model_name = (char *)malloc(model_name_length);
    memcpy(qmn->model_name, tmpData, model_name_length);

    m.unlock();
  }
}

void cInverter::GetQFLAG(QFLAG *qflag)
{
  if (query("QFLAG") &&
      strcmp((char *)&buf[1], "NAK") != 0)
  {
    m.lock();
    char *tmpData = (char *)buf + 1;
    size_t tmpData_length = strlen(tmpData) + 1;

    atomic_bool D_not_found = ATOMIC_VAR_INIT(true);

    for (size_t i = 0; i < tmpData_length; i++)
    {
      switch (tmpData[i])
      {
      case 'E':
        D_not_found.store(true);
        break;

      case 'D':
        D_not_found.store(false);
        break;

      case 'a':
        qflag->silence_open_buzzer.store(D_not_found);
        break;

      case 'b':
        qflag->bypass_function.store(D_not_found);
        break;

      case 'c':
        qflag->bypass_function_forbidden.store(D_not_found);
        break;

      case 'j':
        qflag->power_saving.store(D_not_found);
        break;

      case 'k':
        qflag->lcd_timeout_default_page.store(D_not_found);
        break;

      case 'u':
        qflag->overload_restart.store(D_not_found);
        break;

      case 'v':
        qflag->overtemperature_restart.store(D_not_found);
        break;

      case 'x':
        qflag->lcd_backlight.store(D_not_found);
        break;

      case 'y':
        qflag->alarm_primary_input.store(D_not_found);
        break;

      case 'z':
        qflag->fault_code_record.store(D_not_found);
        break;

      default:
        break;
      }
    }
    m.unlock();
  }
}

void cInverter::GetQID(QID *qid)
{
  if (query("QID") &&
      strcmp((char *)&buf[1], "NAK") != 0)
  {
    m.lock();
    char *tmpData = (char *)buf + 1;

    if (qid->inverter_id != NULL)
    {
      free(qid->inverter_id);
    }

    size_t inverter_id_length = strlen(tmpData) + 1;
    qid->inverter_id = (char *)malloc(inverter_id_length);
    memcpy(qid->inverter_id, tmpData, inverter_id_length);

    m.unlock();
  }
}

bool cInverter::query(const char *cmd)
{
  time_t started;
  int fd;
  int i = 0, n;

  fd = open(this->device.data(), O_RDWR | O_NONBLOCK); // device is provided by program arg (usually /dev/hidraw0)
  if (fd == -1)
  {
    lprintf("DEBUG:  Unable to open device file (errno=%d %s)", errno, strerror(errno));
    sleep(10);
    return false;
  }

  // Once connected, set the baud rate and other serial config (Don't rely on this being correct on the system by default...)
  speed_t baud = B2400;

  // Speed settings (in this case, 2400 8N1)
  struct termios settings;
  tcgetattr(fd, &settings);

  cfsetospeed(&settings, baud); // baud rate
  settings.c_cflag &= ~PARENB;  // no parity
  settings.c_cflag &= ~CSTOPB;  // 1 stop bit
  settings.c_cflag &= ~CSIZE;
  settings.c_cflag |= CS8 | CLOCAL; // 8 bits
  // settings.c_lflag = ICANON;         // canonical mode
  settings.c_oflag &= ~OPOST; // raw output

  tcsetattr(fd, TCSANOW, &settings); // apply the settings
  tcflush(fd, TCOFLUSH);

  // ---------------------------------------------------------------

  // Generating CRC for a command
  uint16_t crc = cal_crc_half((uint8_t *)cmd, strlen(cmd));
  n = strlen(cmd);
  memcpy(&buf, cmd, n);
  lprintf("DEBUG:  Current CRC: %X %X", crc >> 8, crc & 0xff);
  buf[n++] = crc >> 8;
  buf[n++] = crc & 0xff;
  buf[n++] = 0x0d;

  // Send buffer in hex
  char messagestart[128];
  char *messageptr = messagestart;
  sprintf(messagestart, "DEBUG:  Send buffer hex bytes:  ( ");
  messageptr += strlen(messagestart);

  for (int j = 0; j < n; j++)
  {
    int size = sprintf(messageptr, "%02x ", buf[j]);
    messageptr += 3;
  }
  lprintf("%s)", messagestart);

  /* The below command doesn't take more than an 8-byte payload 5 chars (+ 3
     bytes of <CRC><CRC><CR>).  It has to do with low speed USB specifications.
     So we must chunk up the data and send it in a loop 8 bytes at a time.  */

  // Send the command (or part of the command if longer than chunk_size)
  int chunk_size = 8;
  if (n < chunk_size) // Send in chunks of 8 bytes, if less than 8 bytes to send... just send that
    chunk_size = n;
  int bytes_sent = 0;
  int remaining = n;

  while (remaining > 0)
  {
    ssize_t written = write(fd, &buf[bytes_sent], chunk_size);
    bytes_sent += written;
    if (remaining - written >= 0)
      remaining -= written;
    else
      remaining = 0;

    if (written < 0)
      lprintf("DEBUG:  Write command failed, error number %d was returned", errno);
    else
      lprintf("DEBUG:  %d bytes written, %d bytes sent, %d bytes remaining", written, bytes_sent, remaining);

    chunk_size = remaining;
    usleep(50000); // Sleep 50ms before sending another 8 bytes of info
  }

  time(&started);

  // Instead of using a fixed size for expected response length, lets find it
  // by searching for the first returned <cr> char instead.
  char *startbuf = 0;
  char *endbuf = 0;
  do
  {
    // According to protocol manual, it appears no query should ever exceed 120 byte size in response; But this is not true! Thats why its 1000 now!
    n = read(fd, (void *)buf + i, 1000 - i);
    if (n < 0)
    {
      if (time(NULL) - started > 5) // Wait 5 secs before timeout
      {
        lprintf("DEBUG:  %s read timeout", cmd);
        break;
      }
      else
      {
        usleep(50000); // sleep 50ms
        continue;
      }
    }
    i += n;
    buf[i] = '\0'; // terminate what we have so far with a null string
    lprintf("DEBUG:  %d bytes read, %d total bytes:  %02x %02x %02x %02x %02x %02x %02x %02x",
            n, i, buf[i - 8], buf[i - 7], buf[i - 6], buf[i - 5], buf[i - 4], buf[i - 3], buf[i - 2], buf[i - 1]);

    startbuf = (char *)&buf[0];
    endbuf = strchr(startbuf, '\r');
    if (endbuf == NULL && i >= 1000)
    {
      endbuf = (char *)&buf[i - 1];
    }

    // lprintf("DEBUG:  %s Current buffer: %s", cmd, startbuf);
  } while (endbuf == NULL); // Still haven't found end <cr> char as long as pointer is null
  close(fd);

  int replysize = endbuf - startbuf + 1;
  lprintf("DEBUG:  Found reply <cr> at byte: %d", replysize);

  if (buf[0] != '(' || buf[replysize - 1] != 0x0d)
  {
    lprintf("DEBUG:  %s: incorrect buffer start/stop bytes.  Buffer: %s", cmd, buf);
    return false;
  }
  if (!(CheckCRC(buf, replysize)))
  {
    lprintf("DEBUG:  %s: CRC Failed!  Reply size: %d  Buffer: %s", cmd, replysize, buf);
    return false;
  }
  buf[replysize - 3] = '\0'; // Null-terminating on first CRC byte
  lprintf("DEBUG:  %s: %d bytes read: %s", cmd, i, buf);

  lprintf("DEBUG:  %s query finished", cmd);
  return true;
}

void cInverter::poll()
{
  extern const int runOnce;

  while (true)
  {
    if (inv_data_avail)
    {
      if (quit_thread || runOnce)
        return;
    }
    else
    {
      // Get model name of inverter
      GetQMN(&qmn);

      // Get id of inverter
      GetQID(&qid);

      // Get id of inverter
      GetQFLAG(&qflag);

      // Reading mode
      GetQMOD(&qmod);

      // Reading QPIGS status
      GetQPIGSn(&qpigsn);

      // Reading QPIRI status
      GetQPIRI(&qpiri);

      // Get any device warnings...
      GetQPIWS(&qpiws);

      m.lock();
      inv_data_avail = true;
      m.unlock();

      if (quit_thread || runOnce)
        return;
    }

    sleep(5);
  }
}

string *cInverter::ExecuteCmd(const string cmd)
{
  string *return_string;
  // Sending any command raw
  if (query(cmd.data()))
  {
    m.lock();
    return_string = new string((const char *)buf + 1);
    m.unlock();
  }
  return return_string;
}

uint16_t cInverter::cal_crc_half(uint8_t *pin, uint8_t len)
{
  uint16_t crc;

  uint8_t da;
  uint8_t *ptr;
  uint8_t bCRCHign;
  uint8_t bCRCLow;

  uint16_t crc_ta[16] = {
      0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
      0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef};
  ptr = pin;
  crc = 0;

  while (len-- != 0)
  {
    da = ((uint8_t)(crc >> 8)) >> 4;
    crc <<= 4;
    crc ^= crc_ta[da ^ (*ptr >> 4)];
    da = ((uint8_t)(crc >> 8)) >> 4;
    crc <<= 4;
    crc ^= crc_ta[da ^ (*ptr & 0x0f)];
    ptr++;
  }
  bCRCLow = crc;
  bCRCHign = (uint8_t)(crc >> 8);
  if (bCRCLow == 0x28 || bCRCLow == 0x0d || bCRCLow == 0x0a)
    bCRCLow++;
  if (bCRCHign == 0x28 || bCRCHign == 0x0d || bCRCHign == 0x0a)
    bCRCHign++;
  crc = ((uint16_t)bCRCHign) << 8;
  crc += bCRCLow;
  return (crc);
}

bool cInverter::CheckCRC(unsigned char *data, int len)
{
  uint16_t crc = cal_crc_half(data, len - 3);
  return data[len - 3] == (crc >> 8) && data[len - 2] == (crc & 0xff);
}
