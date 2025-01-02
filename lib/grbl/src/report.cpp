/*
  report.c - reporting and messaging methods
  Part of Grbl

  Copyright (c) 2012-2016 Sungeun K. Jeon for Gnea Research LLC

  Grbl is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Grbl is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Grbl.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
  This file functions as the primary feedback interface for Grbl. Any outgoing data, such
  as the protocol status messages, feedback messages, and status reports, are stored here.
  For the most part, these functions primarily are called from protocol.c methods. If a
  different style feedback is desired (i.e. JSON), then a user can change these following
  methods to accomodate their needs.
*/

#include "grbl.hpp"

// Taken from Grbl_Esp32
// this is a generic send function that everything should use, so interfaces could be added (Bluetooth, etc)
void grbl_send(uint8_t client, const char *text)
{
	if ( client == CLIENT_WEBSOCKET || client == CLIENT_ALL )
		Serial2Socket.write((const uint8_t*)text, strlen(text));

	if ( client == CLIENT_SERIAL || client == CLIENT_ALL )
		Serial.print(text);
}

// Taken from Grbl_Esp32
// This is a formating version of the grbl_send(CLIENT_ALL,...) function that work like printf
void grbl_sendf(uint8_t client, const char *format, ...)
{
  char local_buffer[64];
  char * temp = local_buffer;
  va_list argument_list;
  va_list argument_copy;
  va_start(argument_list, format);
  va_copy(argument_copy, argument_list);
  size_t length = vsnprintf(NULL, 0, format, argument_list);
  va_end(argument_copy);
  if(length >= sizeof(local_buffer)){
    temp = new char[length+1];
    if(temp == NULL) {
      return;
    }
  }
  length = vsnprintf(temp, length+1, format, argument_list);
  grbl_send(client, temp);
  va_end(argument_list);
  if(length > 64){
    delete[] temp;
  }
}

// Use to send [MSG:xxxx] Type messages. The level allows messages to be easily suppressed
void grbl_msg_sendf(uint8_t client, uint8_t level, const char *format, ...) {
	if (level > GRBL_MSG_LEVEL) return;

    char local_buffer[64];
    char * temp = local_buffer;
    va_list argument_list;
    va_list argument_copy;
    va_start(argument_list, format);
    va_copy(argument_copy, argument_list);
    size_t length = vsnprintf(NULL, 0, format, argument_list);
    va_end(argument_copy);
    if(length >= sizeof(local_buffer)){
        temp = new char[length+1];
        if(temp == NULL) {
            return;
        }
    }
    length = vsnprintf(temp, length+1, format, argument_list);
    grbl_sendf(client, "[MSG:%s]\r\n", temp);
    va_end(argument_list);
    if(length > 64){
        delete[] temp;
    }
}

// Internal report utilities to reduce flash with repetitive tasks turned into functions.

// formats axis values into a string and returns that string in rpt
static void report_util_axis_values(float *axis_value, char *report) {
  uint8_t index;
  char axis_value_string[10];
  float unit_conversion = 1.0; // unit conversion multiplier..default is mm

  report[0] = '\0';

  if (bit_istrue(settings.flags,BITFLAG_REPORT_INCHES)) {
    unit_conversion = 1.0 / MM_PER_INCH;
  }

  for (index=0; index<N_AXIS; index++) {
    if (bit_istrue(settings.flags,BITFLAG_REPORT_INCHES)) {
      sprintf(axis_value_string, "%4.4f", axis_value[index] * unit_conversion);  // Report inches to 4 decimals
    } else {
      sprintf(axis_value_string, "%4.3f", axis_value[index] * unit_conversion);  // Report mm to 3 decimals
    }
    strcat(report, axis_value_string);

    if (index < (N_AXIS-1)) {
      strcat(report, ",");
    }
  }
}

// Handles the primary confirmation protocol response for streaming interfaces and human-feedback.
// For every incoming line, this method responds with an 'ok' for a successful command or an
// 'error:'  to indicate some error event with the line or some critical system error during
// operation. Errors events can originate from the g-code parser, settings module, or asynchronously
// from a critical error, such as a triggered hard limit. Interface should always monitor for these
// responses.
void report_status_message(uint8_t status_code, uint8_t client)
{
  switch(status_code) {
    case STATUS_OK: // STATUS_OK
      grbl_send(client,"ok\r\n"); break;
    default:
      grbl_sendf(client, "error:%d\r\n", status_code);
  }
}

// Prints alarm messages.
void report_alarm_message(uint8_t alarm_code)
{
  grbl_sendf(CLIENT_ALL, "ALARM:%d\r\n", alarm_code);		// OK to send to all clients
  delay_ms(500); // Force delay to ensure message clears serial write buffer.
}


// Prints feedback messages. This serves as a centralized method to provide additional
// user feedback for things that are not of the status/alarm message protocol. These are
// messages such as setup warnings, switch toggling, and how to exit alarms.
// NOTE: For interfaces, messages are always placed within brackets. And if silent mode
// is installed, the message number codes are less than zero.
void report_feedback_message(uint8_t message_code)
{
  switch(message_code) {
    case MESSAGE_CRITICAL_EVENT:
      grbl_msg_sendf(CLIENT_SERIAL, MSG_LEVEL_INFO, "Reset to continue"); break;
    case MESSAGE_ALARM_LOCK:
      grbl_msg_sendf(CLIENT_SERIAL, MSG_LEVEL_INFO, "'$H'|'$X' to unlock"); break;
    case MESSAGE_ALARM_UNLOCK:
      grbl_msg_sendf(CLIENT_SERIAL, MSG_LEVEL_INFO, "Caution: Unlocked"); break;
    case MESSAGE_ENABLED:
      grbl_msg_sendf(CLIENT_SERIAL, MSG_LEVEL_INFO, "Enabled"); break;
    case MESSAGE_DISABLED:
      grbl_msg_sendf(CLIENT_SERIAL, MSG_LEVEL_INFO, "Disabled"); break;
    case MESSAGE_SAFETY_DOOR_AJAR:
      grbl_msg_sendf(CLIENT_SERIAL, MSG_LEVEL_INFO, "Check door"); break;
    case MESSAGE_CHECK_LIMITS:
      grbl_msg_sendf(CLIENT_SERIAL, MSG_LEVEL_INFO, "Check limits"); break;
    case MESSAGE_PROGRAM_END:
      grbl_msg_sendf(CLIENT_SERIAL, MSG_LEVEL_INFO, "Program End"); break;
    case MESSAGE_RESTORE_DEFAULTS:
      grbl_msg_sendf(CLIENT_SERIAL, MSG_LEVEL_INFO, "Restoring defaults"); break;
    case MESSAGE_SPINDLE_RESTORE:
      grbl_msg_sendf(CLIENT_SERIAL, MSG_LEVEL_INFO, "Restoring spindle");; break;
    case MESSAGE_SLEEP_MODE:
      grbl_msg_sendf(CLIENT_SERIAL, MSG_LEVEL_INFO, "Sleeping"); break;
  }
}

// Welcome message
void report_init_message(uint8_t client)
{
  grbl_send(client,"\r\nGrbl " GRBL_VERSION " ['$' for help]\r\n");
}

// Grbl help message
void report_grbl_help(uint8_t client) {
  grbl_send(client,"[HLP:$$ $+ $# $G $I $N $x=val $Nx=line $J=line $SLP $C $X $H ~ ! ? ctrl-x]\r\n");
}

// Grbl global settings print out.
// NOTE: The numbering scheme here must correlate to storing in settings.c
void report_grbl_settings(uint8_t client) {
  // Print Grbl settings.
  char setting[20];
  char report[1000];

  report[0] = '\0';
  sprintf(setting, "$0=%d\r\n", settings.pulse_microseconds); strcat(report, setting);
  sprintf(setting, "$1=%d\r\n", settings.stepper_idle_lock_time);  strcat(report, setting);
  sprintf(setting, "$2=%d\r\n", settings.step_invert_mask);  strcat(report, setting);
  sprintf(setting, "$3=%d\r\n", settings.dir_invert_mask);  strcat(report, setting);
  sprintf(setting, "$4=%d\r\n", bit_istrue(settings.flags,BITFLAG_INVERT_ST_ENABLE));  strcat(report, setting);
  sprintf(setting, "$5=%d\r\n", bit_istrue(settings.flags,BITFLAG_INVERT_LIMIT_PINS));  strcat(report, setting);
  sprintf(setting, "$6=%d\r\n", bit_istrue(settings.flags,BITFLAG_INVERT_PROBE_PIN));  strcat(report, setting);
  sprintf(setting, "$10=%d\r\n", settings.status_report_mask);  strcat(report, setting);

  sprintf(setting, "$11=%4.3f\r\n", settings.junction_deviation);   strcat(report, setting);
  sprintf(setting, "$12=%4.3f\r\n", settings.arc_tolerance);   strcat(report, setting);

  sprintf(setting, "$13=%d\r\n", bit_istrue(settings.flags,BITFLAG_REPORT_INCHES));   strcat(report, setting);
  sprintf(setting, "$20=%d\r\n", bit_istrue(settings.flags,BITFLAG_SOFT_LIMIT_ENABLE));   strcat(report, setting);
  sprintf(setting, "$21=%d\r\n", bit_istrue(settings.flags,BITFLAG_HARD_LIMIT_ENABLE));   strcat(report, setting);
  sprintf(setting, "$22=%d\r\n", bit_istrue(settings.flags,BITFLAG_HOMING_ENABLE));   strcat(report, setting);
  sprintf(setting, "$23=%d\r\n", settings.homing_dir_mask);   strcat(report, setting);

  sprintf(setting, "$24=%4.3f\r\n", settings.homing_feed_rate);   strcat(report, setting);
  sprintf(setting, "$25=%4.3f\r\n", settings.homing_seek_rate);   strcat(report, setting);
  sprintf(setting, "$26=%d\r\n", settings.homing_debounce_delay);   strcat(report, setting);

  sprintf(setting, "$27=%4.3f\r\n", settings.homing_pulloff);   strcat(report, setting);
  sprintf(setting, "$30=%4.3f\r\n", settings.rpm_max);   strcat(report, setting);
  sprintf(setting, "$31=%4.3f\r\n", settings.rpm_min);   strcat(report, setting);

#ifdef VARIABLE_SPINDLE
  sprintf(setting, "$32=%d\r\n", bit_istrue(settings.flags,BITFLAG_LASER_MODE));  strcat(report, setting);
#else
  strcat(report, "$32=0\r\n");
#endif

  // Print axis settings
  uint8_t index, setting_index;
  uint8_t value = AXIS_SETTINGS_START_VAL;
  for (setting_index=0; setting_index<AXIS_N_SETTINGS; setting_index++) {
    for (index=0; index<N_AXIS; index++) {
      switch (setting_index) {
        case 0: sprintf(setting, "$%d=%4.3f\r\n", value+index, settings.steps_per_mm[index]);   strcat(report, setting);	 break;
        case 1: sprintf(setting, "$%d=%4.3f\r\n", value+index, settings.max_rate[index]);   strcat(report, setting);	 break;
        case 2: sprintf(setting, "$%d=%4.3f\r\n", value+index, settings.acceleration[index]/(60*60));   strcat(report, setting);	 break;
        case 3: sprintf(setting, "$%d=%4.3f\r\n", value+index, -settings.max_travel[index]);   strcat(report, setting);	 break;
      }
    }
    value += AXIS_SETTINGS_INCREMENT;
  }
  grbl_send(client,report);
}


// Prints current probe parameters. Upon a probe command, these parameters are updated upon a
// successful probe or upon a failed probe with the G38.3 without errors command (if supported).
// These values are retained until Grbl is power-cycled, whereby they will be re-zeroed.
void report_probe_parameters(uint8_t client)
{
  // Report in terms of machine position.
  float print_position[N_AXIS];
  char probe_report[120];	// the probe report we are building here
  char temp[60];

  strcpy(probe_report, "[PRB:"); // initialize the string with the first characters

  // get the machine position and put them into a string and append to the probe report
  system_convert_array_steps_to_mpos(print_position,sys_probe_position);
  report_util_axis_values(print_position, temp);
  strcat(probe_report, temp);

  // add the success indicator and add closing characters
  sprintf(temp, ":%d]\r\n", sys.probe_succeeded);
  strcat(probe_report, temp);

  grbl_send(client, probe_report); // send the report
}

// Prints Grbl NGC parameters (coordinate offsets, probing)
void report_ngc_parameters(uint8_t client)
{
  float coordinate_data[N_AXIS];
  uint8_t coordinate_select;
  char temp[60];
  char ngc_report[500];

  ngc_report[0] = '\0';

  for (coordinate_select = 0; coordinate_select <= SETTING_INDEX_NCOORD; coordinate_select++) {
    if (!(settings_read_coord_data(coordinate_select,coordinate_data))) {
      report_status_message(STATUS_SETTING_READ_FAIL, client);
      return;
    }
	strcat(ngc_report, "[G");
    switch (coordinate_select) {
      case 6: strcat(ngc_report, "28"); break;
      case 7: strcat(ngc_report, "30"); break;
      default:
        sprintf(temp, "%d", coordinate_select+54);
        strcat(ngc_report, temp);
        break; // G54-G59
    }
    strcat(ngc_report, ":");
    report_util_axis_values(coordinate_data, temp);
    strcat(ngc_report, temp);
    strcat(ngc_report, "]\r\n");
  }

  strcat(ngc_report, "[G92:"); // Print G92,G92.1 which are not persistent in memory
  report_util_axis_values(gc_state.coord_offset, temp);
  strcat(ngc_report, temp);
  strcat(ngc_report, "]\r\n");
  strcat(ngc_report, "[TLO:"); // Print tool length offset value

  if (bit_istrue(settings.flags,BITFLAG_REPORT_INCHES)) {
    sprintf(temp, "%4.3f]\r\n", gc_state.tool_length_offset * INCH_PER_MM);
  } else {
    sprintf(temp, "%4.3f]\r\n", gc_state.tool_length_offset);
  }
  strcat(ngc_report, temp);

  grbl_send(client, ngc_report);

  report_probe_parameters(client);
}


// Print current gcode parser mode state
void report_gcode_modes(uint8_t client)
{
  char temp[20];
  char modes_report[75];

  strcpy(modes_report, "[GC:G");

  if (gc_state.modal.motion >= MOTION_MODE_PROBE_TOWARD) {
    sprintf(temp, "38.%d", gc_state.modal.motion - (MOTION_MODE_PROBE_TOWARD-2));
  } else {
    sprintf(temp, "%d", gc_state.modal.motion);
  }
  strcat(modes_report, temp);

  sprintf(temp, " G%d", gc_state.modal.coord_select+54);
  strcat(modes_report, temp);

  sprintf(temp, " G%d", gc_state.modal.plane_select+17);
  strcat(modes_report, temp);

  sprintf(temp, " G%d", 21-gc_state.modal.units);
  strcat(modes_report, temp);

  sprintf(temp, " G%d", gc_state.modal.distance+90);
  strcat(modes_report, temp);

  sprintf(temp, " G%d", 94-gc_state.modal.feed_rate);
  strcat(modes_report, temp);

  if (gc_state.modal.program_flow) {
    switch (gc_state.modal.program_flow) {
      case PROGRAM_FLOW_PAUSED: strcat(modes_report, " M0"); break;
      // case PROGRAM_FLOW_OPTIONAL_STOP: serial_write('1'); break; // M1 is ignored and not supported.
      case PROGRAM_FLOW_COMPLETED_M2:
      case PROGRAM_FLOW_COMPLETED_M30:
        sprintf(temp, " M%d", gc_state.modal.program_flow);
        strcat(modes_report, temp);
        break;
    }
  }

  switch (gc_state.modal.spindle) {
    case SPINDLE_ENABLE_CW : strcat(modes_report, " M3"); break;
    case SPINDLE_ENABLE_CCW : strcat(modes_report, " M4"); break;
    case SPINDLE_DISABLE : strcat(modes_report, " M5"); break;
  }

#ifdef ENABLE_M7
  //report_util_gcode_modes_M();  // optional M7 and M8 should have been dealt with by here
  if (gc_state.modal.coolant) { // Note: Multiple coolant states may be active at the same time.
    if (gc_state.modal.coolant & PL_COND_FLAG_COOLANT_MIST) { strcat(modes_report, " M7"); }
    if (gc_state.modal.coolant & PL_COND_FLAG_COOLANT_FLOOD) { strcat(modes_report, " M8"); }
  } else { strcat(modes_report, " M9"); }
#else
  if (gc_state.modal.coolant) {
    strcat(modes_report, " M8");
  } else {
    strcat(modes_report, " M9");
  }
#endif

  #ifdef ENABLE_PARKING_OVERRIDE_CONTROL
    if (sys.override_ctrl == OVERRIDE_PARKING_MOTION) {
      strcat(modes_report, " M56");
    }
  #endif

  sprintf(temp, " T%d", gc_state.tool);
  strcat(modes_report, temp);

  if (bit_istrue(settings.flags,BITFLAG_REPORT_INCHES)) {
    sprintf(temp, " F%.1f", gc_state.feed_rate);
  } else {
    sprintf(temp, " F%.0f", gc_state.feed_rate);
  }
  strcat(modes_report, temp);

#ifdef VARIABLE_SPINDLE
  sprintf(temp, " S%4.3f", gc_state.spindle_speed);
  strcat(modes_report, temp);
#endif

  strcat(modes_report, "]\r\n");

  grbl_send(client, modes_report);
}


// Prints specified startup line
void report_startup_line(uint8_t n, char *line, uint8_t client)
{
	grbl_sendf(client, "$N%d=%s\r\n", n, line);	// OK to send to all
}

void report_execute_startup_message(char *line, uint8_t status_code, uint8_t client)
{
  grbl_sendf(client, ">%s:", line);  // OK to send to all
  report_status_message(status_code, client);
}

// Prints build info line
void report_build_info(char *line, uint8 client)
{
  char build_info[50];

  strcpy(build_info, "[VER:" GRBL_VERSION "." GRBL_VERSION_BUILD ":");
  strcat(build_info, line);
  strcat(build_info, "]\r\n[OPT:");

  #ifdef VARIABLE_SPINDLE
    strcat(build_info,"V");
  #endif
  #ifdef USE_LINE_NUMBERS
    strcat(build_info,"N");
  #endif
  #ifdef COOLANT_MIST_PIN
    strcat(build_info,"M"); // TODO Need to deal with M8...it could be disabled
  #endif
  #ifdef COREXY
    strcat(build_info,"C");
  #endif
  #ifdef PARKING_ENABLE
    strcat(build_info,"P");
  #endif
  #if (defined(HOMING_FORCE_SET_ORIGIN) || defined(HOMING_FORCE_POSITIVE_SPACE))
    strcat(build_info,"Z"); // homing MPOS bahavior is not the default behavior
  #endif
  #ifdef HOMING_SINGLE_AXIS_COMMANDS
    strcat(build_info,"H");
  #endif
  #ifdef LIMITS_TWO_SWITCHES_ON_AXES
    strcat(build_info,"L");
  #endif
  #ifdef ALLOW_FEED_OVERRIDE_DURING_PROBE_CYCLES
    strcat(build_info,"A");
  #endif
  #if defined (ENABLE_WIFI)
    strcat(build_info,"W");
  #endif
  #ifndef ENABLE_RESTORE_EEPROM_WIPE_ALL // NOTE: Shown when disabled.
    strcat(build_info,"*");
  #endif
  #ifndef ENABLE_RESTORE_EEPROM_DEFAULT_SETTINGS // NOTE: Shown when disabled.
    strcat(build_info,"$");
  #endif
  #ifndef ENABLE_RESTORE_EEPROM_CLEAR_PARAMETERS // NOTE: Shown when disabled.
    strcat(build_info,"#");
  #endif
  #ifndef ENABLE_BUILD_INFO_WRITE_COMMAND // NOTE: Shown when disabled.
    strcat(build_info,"I");
  #endif
  #ifndef FORCE_BUFFER_SYNC_DURING_EEPROM_WRITE // NOTE: Shown when disabled.
    strcat(build_info,"E");
  #endif
  #ifndef FORCE_BUFFER_SYNC_DURING_WCO_CHANGE // NOTE: Shown when disabled.
    strcat(build_info,"W");
  #endif
  // NOTE: Compiled values, like override increments/max/min values, may be added at some point later.
  // These will likely have a comma delimiter to separate them.

  strcat(build_info,"]\r\n");
  grbl_send(client, build_info); // ok to send to all
  #if defined (ENABLE_WIFI)
    grbl_send(client, (char *)wifi_config.info());
  #endif

  #ifdef LIMITS_TWO_SWITCHES_ON_AXES
    strcat(build_info,"L");
  #endif
}


// Prints the character string line Grbl has received from the user, which has been pre-parsed,
// and has been sent into protocol_execute_line() routine to be executed by Grbl.
void report_echo_line_received(char *line, uint8_t client)
{
	grbl_sendf(client, "[echo: %s]\r\n", line);
}


 // Prints real-time data. This function grabs a real-time snapshot of the stepper subprogram
 // and the actual location of the CNC machine. Users may change the following function to their
 // specific needs, but the desired real-time data report must be as short as possible. This is
 // requires as it minimizes the computational overhead and allows grbl to keep running smoothly,
 // especially during g-code programs with fast, short line segments and high frequency reports (5-20Hz).
void report_realtime_status(uint8_t client)
{
  uint8_t index;
  int32_t current_position[N_AXIS]; // Copy current state of the system position variable
  memcpy(current_position,sys_position,sizeof(sys_position));
  float print_position[N_AXIS];
  char status[200];
  char temp[80];

  system_convert_array_steps_to_mpos(print_position,current_position);

  // Report current machine state and sub-states
  strcpy(status, "<");
  switch (sys.state) {
    case STATE_IDLE: strcat(status, "Idle"); break;
    case STATE_CYCLE: strcat(status, "Run"); break;
    case STATE_HOLD:
      if (!(sys.suspend & SUSPEND_JOG_CANCEL)) {
        strcat(status, "Hold:");
        if (sys.suspend & SUSPEND_HOLD_COMPLETE) { strcat(status, "0"); } // Ready to resume
        else { strcat(status, "1"); } // Actively holding
        break;
      } // Continues to print jog state during jog cancel.
    case STATE_JOG: strcat(status, "Jog"); break;
    case STATE_HOMING: strcat(status, "Home"); break;
    case STATE_ALARM: strcat(status, "Alarm"); break;
    case STATE_CHECK_MODE: strcat(status, "Check"); break;
    case STATE_SAFETY_DOOR:
      strcat(status, "Door:");
      if (sys.suspend & SUSPEND_INITIATE_RESTORE) {
        strcat(status, "3"); // Restoring
      } else {
        if (sys.suspend & SUSPEND_RETRACT_COMPLETE) {
          if (sys.suspend & SUSPEND_SAFETY_DOOR_AJAR) {
            strcat(status, "1"); // Door ajar
          } else {
            strcat(status, "0");
          } // Door closed and ready to resume
        } else {
          strcat(status, "2"); // Retracting
        }
      }
      break;
    case STATE_SLEEP: strcat(status, "Sleep"); break;
  }

  float work_coordinate_offsets[N_AXIS];
  if (bit_isfalse(settings.status_report_mask,BITFLAG_RT_STATUS_POSITION_TYPE) ||
      (sys.report_wco_counter == 0) ) {
    for (index=0; index< N_AXIS; index++) {
      // Apply work coordinate offsets and tool length offset to current position.
      work_coordinate_offsets[index] = gc_state.coord_system[index]+gc_state.coord_offset[index];
      if (index == TOOL_LENGTH_OFFSET_AXIS) { work_coordinate_offsets[index] += gc_state.tool_length_offset; }
      if (bit_isfalse(settings.status_report_mask,BITFLAG_RT_STATUS_POSITION_TYPE)) {
        print_position[index] -= work_coordinate_offsets[index];
      }
    }
  }
  // Report machine position
  if (bit_istrue(settings.status_report_mask,BITFLAG_RT_STATUS_POSITION_TYPE)) {
    strcat(status, "|MPos:");
  } else {
	#ifdef FWD_KINEMATICS_REPORTING
		forward_kinematics(print_position);
	#endif
    strcat(status, "|WPos:");
  }
  report_util_axis_values(print_position, temp);
  strcat(status, temp);

  // Returns planner and serial read buffer states.
  #ifdef REPORT_FIELD_BUFFER_STATE
  if (bit_istrue(settings.status_report_mask,BITFLAG_RT_STATUS_BUFFER_STATE)) {
    int buffer_size = serial_get_rx_buffer_available(client);
    sprintf(temp, "|Bf:%d,%d", plan_get_block_buffer_available(), buffer_size);
    strcat(status, temp);
  }
  #endif

  #ifdef USE_LINE_NUMBERS
    #ifdef REPORT_FIELD_LINE_NUMBERS
      // Report current line number
      plan_block_t * current_block = plan_get_current_block();
      if (current_block != NULL) {
        uint32_t line_number = current_block->line_number;
        if (line_number > 0) {
					sprintf(temp, "|Ln:%d", line_number);
					strcat(status, temp);
        }
      }
    #endif
  #endif

  // Report realtime feed speed
  #ifdef REPORT_FIELD_CURRENT_FEED_SPEED
    #ifdef VARIABLE_SPINDLE
      if (bit_istrue(settings.flags,BITFLAG_REPORT_INCHES)) {
        sprintf(temp, "|FS:%.1f,%.0f", st_get_realtime_rate(), sys.spindle_speed / MM_PER_INCH);
      } else {
        sprintf(temp, "|FS:%.0f,%.0f", st_get_realtime_rate(), sys.spindle_speed);
      }
      strcat(status, temp);
    #else
      if (bit_istrue(settings.flags,BITFLAG_REPORT_INCHES)) {
        sprintf(temp, "|F:%.1f", st_get_realtime_rate() / MM_PER_INCH);
      } else {
        sprintf(temp, "|F:%.0f", st_get_realtime_rate());
      }
      strcat(status, temp);
    #endif
  #endif

  #ifdef REPORT_FIELD_PIN_STATE
    uint8_t limit_pin_state = limits_get_state();
    uint8_t control_pin_state = system_control_get_state();
    uint8_t probe_pin_state = probe_get_state();
    if (limit_pin_state | control_pin_state | probe_pin_state) {
      strcat(status, "|Pn:");
      if (probe_pin_state) { strcat(status, "P"); }
      if (limit_pin_state) {
        if (bit_istrue(limit_pin_state,bit(X_AXIS))) { strcat(status, "X"); }
        if (bit_istrue(limit_pin_state,bit(Y_AXIS))) { strcat(status, "Y"); }
        if (bit_istrue(limit_pin_state,bit(Z_AXIS))) { strcat(status, "Z"); }
        if (bit_istrue(limit_pin_state,bit(A_AXIS))) { strcat(status, "A"); }
        if (bit_istrue(limit_pin_state,bit(B_AXIS))) { strcat(status, "B"); }
        if (bit_istrue(limit_pin_state,bit(C_AXIS))) { strcat(status, "C"); }
        if (bit_istrue(limit_pin_state,bit(D_AXIS))) { strcat(status, "D"); }
        if (bit_istrue(limit_pin_state,bit(E_AXIS))) { strcat(status, "E"); }
      }
      if (control_pin_state) {
        #ifdef ENABLE_SAFETY_DOOR_INPUT_PIN
          if (bit_istrue(control_pin_state,CONTROL_PIN_INDEX_SAFETY_DOOR)) { strcat(status, "D"); }
        #endif
        if (bit_istrue(control_pin_state,CONTROL_PIN_INDEX_RESET)) { strcat(status, "R"); }
        if (bit_istrue(control_pin_state,CONTROL_PIN_INDEX_FEED_HOLD)) { strcat(status, "H"); }
        if (bit_istrue(control_pin_state,CONTROL_PIN_INDEX_CYCLE_START)) { strcat(status, "S"); }
      }
    }
  #endif

  #ifdef REPORT_FIELD_WORK_COORD_OFFSET
    if (sys.report_wco_counter > 0) { sys.report_wco_counter--; }
    else {
      if (sys.state & (STATE_HOMING | STATE_CYCLE | STATE_HOLD | STATE_JOG | STATE_SAFETY_DOOR)) {
        sys.report_wco_counter = (REPORT_WCO_REFRESH_BUSY_COUNT-1); // Reset counter for slow refresh
      } else { sys.report_wco_counter = (REPORT_WCO_REFRESH_IDLE_COUNT-1); }
      if (sys.report_ovr_counter == 0) { sys.report_ovr_counter = 1; } // Set override on next report.
      strcat(status, "|WCO:");
      report_util_axis_values(work_coordinate_offsets, temp);
	  strcat(status, temp);
    }
  #endif

  #ifdef REPORT_FIELD_OVERRIDES
    if (sys.report_ovr_counter > 0) { sys.report_ovr_counter--; }
    else {
      if (sys.state & (STATE_HOMING | STATE_CYCLE | STATE_HOLD | STATE_JOG | STATE_SAFETY_DOOR)) {
        sys.report_ovr_counter = (REPORT_OVR_REFRESH_BUSY_COUNT-1); // Reset counter for slow refresh
      } else { sys.report_ovr_counter = (REPORT_OVR_REFRESH_IDLE_COUNT-1); }
      sprintf(temp, "|Ov:%d,%d,%d", sys.f_override, sys.r_override, sys.spindle_speed_ovr);
      strcat(status, temp);

      uint8_t spindle_state = spindle_get_state();
      uint8_t coolant_state = coolant_get_state();
      if (spindle_state || coolant_state) {
        strcat(status, "|A:");
        if (spindle_state) { // != SPINDLE_STATE_DISABLE
          if (spindle_state == SPINDLE_STATE_CW) { strcat(status, "S"); } // CW
          else { strcat(status, "C"); } // CCW
        }
        if (coolant_state & COOLANT_STATE_FLOOD) { strcat(status, "F"); }
        #ifdef COOLANT_MIST_PIN // TODO Deal with M8 - Flood
          if (coolant_state & COOLANT_STATE_MIST) { strcat(status, "M"); }
        #endif
      }
    }
  #endif

  strcat(status, ">\r\n");
  grbl_send(client, status);
}

#ifdef DEBUG
  void report_realtime_debug()
  {

  }
#endif
