// 
// Copyright 2015 aweatherguy
// 
// This file is part of ABSONS which is an acronym for
// "Arduino-Based Stratum One Ntp Server".
// 
// ABSONS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// 
// =========================================================================
// 
// 

#pragma once

#if SYSCLK_DISCIPLINED
//
// ========================== TIME UNITS =============================
//
// these values need to be all edited in unison if the configuration is changed.
// it would be possible to write macros to make it easier, 
// but it's not really all that much work. just get it right.
//
// Based the following setup:
// Processor clock          16MHz
// Timer clock              2MHz ==> must set prescaler for T4/T5 to divide by 8
// Nominal timer interval   62,500 counts
// Nominal timer cycle      31.25ms (1/32 Hz) equal to nominal interval/clock = 62,500/2MHz
// 
// NTP time conversions are for the fractional part of an NTP time value,
// for which 2^32 <==> 1 second <==> 10^9 ns.
//

//
// the choice to keep time in ns is somewhat arbitrary and probably provides more
// resolution than is really necessary. This would probably still work fine if
// the units were 10's of ns instead for example.
//
// These values are dependent on F_CPU and apply to 10MHz clock frequency only, so we 
// squawk if that's not what we have.
//
#if F_CPU != 10000000UL
#error "Invalid value for F_CPU: values in this file are only valid for a 10MHz system clock"
#endif
//
// number of NS per nominal timer clock tick. for a nominal 10MHz clock this is 100ns per tick.
//
#define NOM_TICK_NS     100UL
//
// nominal time clock tick in fractional NTP seconds.
// this is not exact: the real value (2^32 x 100e-9) is closer to 429.4967....
// don't use this for anything but converting small time quantities
// from timer counts to NTP fractional units.
//
#define NOM_TICK_NTP    429U
//
// this is the nominal timer cycle length in clock ticks.
// for a 10MHz clock this gives 160 timer cycles per second.
// or 6.250ms per cycle.
//
#define NOMINAL_INTEGER_DIVISOR    62500U
//
// preset value for timers during PPS capture interrupt, includes
// about 8us of interrupt latency to get the actual value at the 
// next pps capture very close to half-way
//
#define PPS_TIMER_PRESET            31329U
//
// length of a nominal timer cycle in ns and NTP fractional seconds
// this is just the time required for NOMINAL_INTEGER_DIVISION_TICKS 
// at the nominal time clock frequency.
//
#define NOM_CYCLE_NS        6250000UL
#define NOM_CYCLE_NTP       26843546UL      /* not exact, true value is 26843545.6 */
#define NOM_CYCLE_PER_SEC   160U

#define NTP_WINDOW_US   3000UL
//
// These offsetsare added to timer values to account for PPS signals arriving half-way through a timer cycle
//
#define PPS_OFFSET_NS       3125000UL
#define PPS_OFFSET_NTP      13958644UL      /* not exact: actual value is 13958643.712 */
//
// the amount of time we add to current time when building a transmit timestamp.
// this is fine-tuned below to account for extra latency in sending the start command to W5500
// and code execution time.
// 
#define NTP_TX_DELAY_US           800U       /* how long after getting current time will we kick off packet transmission? */
#define NTP_TX_DELAY_TICKS       8000U       /* in 10MHz timer ticks */
//
// =========== 10MHz Mega2560r3 Measurements (SPI at F_CPU/2) =======================
//
// The current code shows a variation of perhaps +-400ns in these
// comparisons which is likely due to variations in latency in the loop waiting for proper time
// to send the packet...
// The wait loop appears to require 8 clock cycles so the +-400ns is just right for variation.
//
// time measurement from the call to endPacket and SEND_OK interrupt asserted is 29.1us. 
// To this must be added a little more (1.2us) to get the scope measurements to match.
//
#define NTP_TX_FINE_ADJUST_NS   (29100UL + 1370UL)
#define NTP_TX_ADJUST_NS        (1000L * NTP_TX_DELAY_US + NTP_TX_FINE_ADJUST_NS)
#define NTP_TX_ADJUST_NTP       (OFFSET_NS_2_NTP(NTP_TX_ADJUST_NS))

#endif
