/*
  nuts_bolts.c - Shared functions
  Part of Grbl

  Copyright (c) 2011-2016 Sungeun K. Jeon for Gnea Research LLC
  Copyright (c) 2009-2011 Simen Svale Skogsrud

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

#include "grbl.hpp"

#define MAX_INT_DIGITS 8 // Maximum number of digits in int32 (and float)


// Extracts a floating point value from a string. The following code is based loosely on
// the avr-libc strtod() function by Michael Stumpf and Dmitry Xmelkov and many freely
// available conversion method examples, but has been highly optimized for Grbl. For known
// CNC applications, the typical decimal value is expected to be in the range of E0 to E-4.
// Scientific notation is officially not supported by g-code, and the 'E' character may
// be a g-code word on some CNC systems. So, 'E' notation will not be recognized.
// NOTE: Thanks to Radu-Eosif Mihailescu for identifying the issues with using strtod().
uint8_t read_float(char *line, uint8_t *char_counter, float *float_ptr)
{
  char *ptr = line + *char_counter;
  unsigned char current_char;

  // Grab first character and increment pointer. No spaces assumed in line.
  current_char = *ptr++;

  // Capture initial positive/minus character
  bool is_negative = false;
  if (current_char == '-') {
    is_negative = true;
    current_char = *ptr++;
  } else if (current_char == '+') {
    current_char = *ptr++;
  }

  // Extract number into fast integer. Track decimal in terms of exponent value.
  uint32_t integer_value = 0;
  int88_t exponent = 0;
  uint8_t num_digits = 0;
  bool is_decimal = false;
  while(1) {
    current_char -= '0';
    if (current_char <= 9) {
      num_digits++;
      if (num_digits <= MAX_INT_DIGITS) {
        if (is_decimal) { exponent--; }
        integer_value = (((integer_value << 2) + integer_value) << 1) + current_char; // integer_value*10 + current_char
      } else {
        if (!(is_decimal)) { exponent++; }  // Drop overflow digits
      }
    } else if (current_char == (('.'-'0') & 0xff)  &&  !(is_decimal)) {
      is_decimal = true;
    } else {
      break;
    }
    current_char = *ptr++;
  }

  // Return if no digits have been read.
  if (!num_digits) { return(false); };

  // Convert integer into floating point.
  float float_value;
  float_value = (float)integer_value;

  // Apply decimal. Should perform no more than two floating point multiplications for the
  // expected range of E0 to E-4.
  if (float_value != 0) {
    while (exponent <= -2) {
      float_value *= 0.01;
      exponent += 2;
    }
    if (exponent < 0) {
      float_value *= 0.1;
    } else if (exponent > 0) {
      do {
        float_value *= 10.0;
      } while (--exponent > 0);
    }
  }

  // Assign floating point value with correct sign.
  if (is_negative) {
    *float_ptr = -float_value;
  } else {
    *float_ptr = float_value;
  }

  *char_counter = ptr - line - 1; // Set char_counter to next statement

  return(true);
}


// Non-blocking delay function used for general operation and suspend features.
void delay_sec(float seconds, uint8_t mode)
{
 	uint16_t iterations = ceil(1000/DWELL_TIME_STEP*seconds);
	while (iterations-- > 0) {
    ESP.wdtFeed();
		if (sys.abort) { return; }
		if (mode == DELAY_MODE_DWELL) {
			protocol_execute_realtime();
		} else { // DELAY_MODE_SYS_SUSPEND
		  // Execute rt_system() only to avoid nesting suspend loops.
		  protocol_exec_rt_system();
		  if (sys.suspend & SUSPEND_RESTART_RETRACT) { return; } // Bail, if safety door reopens.
		}
		delay_ms(DWELL_TIME_STEP); // Delay DWELL_TIME_STEP increment
	}
}

// Delays variable defined milliseconds. Compiler compatibility fix for _delay_ms(),
// which only accepts constants in future compiler releases.
void delay_ms(uint16_t milliseconds)
{
  while ( milliseconds-- ) { ESP.wdtFeed(); delayMicroseconds(950); }
}

// Simple hypotenuse computation function.
float hypot_f(float x, float y) { return(sqrt(x*x + y*y)); }


float convert_delta_vector_to_unit_vector(float *vector)
{
  uint8_t idx;
  float magnitude = 0.0;
  for (idx=0; idx<N_AXIS; idx++) {
    if (vector[idx] != 0.0) {
      magnitude += vector[idx]*vector[idx];
    }
  }
  magnitude = sqrt(magnitude);
  float inv_magnitude = 1.0/magnitude;
  for (idx=0; idx<N_AXIS; idx++) { vector[idx] *= inv_magnitude; }
  return(magnitude);
}


float limit_value_by_axis_maximum(float *max_value, float *unit_vec)
{
  uint8_t idx;
  float limit_value = SOME_LARGE_VALUE;
  for (idx=0; idx<N_AXIS; idx++) {
    if (unit_vec[idx] != 0) {  // Avoid divide by zero.
      limit_value = min((double)limit_value,fabs(max_value[idx]/unit_vec[idx]));
    }
  }
  return(limit_value);
}
