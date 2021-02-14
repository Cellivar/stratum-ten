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

#if ! SYSCLK_DISCIPLINED

#define HOLDOVER_PREDICTION  1
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
// These values are dependent on F_CPU and apply to 16MHz clock frequency only, so we 
// squawk if that's not what we have.
//
#if F_CPU != 16000000UL
#error "Invalid value for F_CPU: values in this file are only valid for a 16MHz system clock"
#endif
//
// this computes x * 2^32/10^9 = 4.2950...
// which also is about 4 + 151/2^9 + 381/2^23  (within 1/2ppb)
// this macro is good for offsets up to about 11ms (11e6 ns) 
// in ns the limit is 2^32/381 -- the largest multiplier of 381 which is in range
// more tweaks could make it work for larger offsets if necessary, but for now we're only
// using it for values less than 1ms.
//
// number of NS per nominal timer clock tick. for a nominal 2MHz clock this is 500ns per tick.
//
#define NOM_TICK_NS     500UL
//
// nominal time clock tick in fractional NTP seconds.
// this is not exact: the real value (2^32 x 500e-9) is closer to 2147.483648...
// don't use this for anything but converting small time quantities
// from timer counts to NTP fractional units.
//
#define NOM_TICK_NTP    2147U
//
// this is the nominal timer cycle length in clock ticks.
// for a 2MHz clock this gives 32 cycles per second.
//
#define NOMINAL_INTEGER_DIVISOR    62500U
//
// length of a nominal timer cycle in ns and NTP fractional seconds
// this is just the time required for NOMINAL_INTEGER_DIVISION_TICKS 
// at the nominal time clock frequency.
//
#define NOM_CYCLE_NS        31250000UL
#define NOM_CYCLE_NTP       0x08000000UL
#define NOM_CYCLE_PER_SEC   32U

#define NTP_WINDOW_US   26000UL
//
// ============================= PLL Parameters and Loop Tuning =============================
//
// The PLL is designed to regulate phase such that the PPS signal occurs half-way
// between two timer match interrupts. This prevents the two ISRs from overlapping 
// which would mess things up significantly. It also guarantees a clear space between
// interrupts of 15msec or so in which background tasks may run w/o fear of volatile
// variables being altered by ISRs.
//
// The PLL code adds these offsets to timer values before servo-ing the phase, 
// so they must also be added to timer values when creating NTP timestamps.
//
#define PLL_OFFSET_NS       15625000UL
#define PLL_OFFSET_NTP      0x04000000UL
//
// ========== Loop Tuning Parameters =================
//
// These have been hand tuned to give good performance.
// If you understand phase-locked loops and control system
// theory, feel free to play. Otherwise, they are best left alone.
//
// -----> PLL Feedback Units <-----
//
// All of these parameters relate to the units in which the frac-N divisor
// is specified and are relative to the nominal division ratio of NOMINAL_INTEGER_DIVISOR
//
// Let the frac-N division ratio be specified as "m0 + m + k/n". 
// m0 is NOMINAL_INTEGER_DIVISOR (e.g. 62,500) and n is fixed at 2048.
//
// Loop feedback adjusts the division ratio by supplying "m + k/n" and with n = 2048,
// this value is encoded as a signed 32-bit fixed point fraction with 11 fractional bits.
//
// For example, consider the feedback value of 4321 (decimal):
// 4321 = 2 x 2048 + 225. This feedback value will increment the timer interval by two full
// clocks (e.g. up to 62,502) and add 225/2048 fractional ticks per timer cycle. The actual
// division ratio will be 62502+225/2048 = approx 62502.1099 
// 
// The parameters below are used to multiply phase errors in ns and produce a correction
// to the fracN division ratio in the form of a fixed point fraction with 11 fractional bits.
//
// Due to overall loop unit scaling, the integral and proportional gains are specified
// as the denominator of a fraction. 
// For example, a proportional divisor of 70 represents a proportional gain of 1/70.
//
#define PLL_INTEGRATOR_LIMIT (PLL_INTEGRAL_DIV * 25L)    /* limit in ns */
#define PLL_INTEGRAL_DIV 6000L                           /* integral gain */
#define PLL_PROPORTIONAL_DIV 75L                         /* proportional gain */
//
// this limit on control effort is appropriate when the PLL never gets more than say 100us
// out of phase lock. we guarantee this by forcing timers to agree with PPS during 
// initialization. if situations occur where phase errors get larger than this, then
// this value may need to be increased or lock-up may take too long to occur.
//
#define PLL_MAX_CTRL_EFFORT 200L     
//
// ===================== NTP Server Tuning ========================
//
// In the world of NTP, these values are sometimes referred to as "fudge".
// That's a vague and ambiguous term and it is not used in this program. 
// These values are OFFSETS, carefully adjusted to make NTP packet timestamps accurate,
// and special care is needed with transmit timestamps since they are not 
// generated by hardware.
//
// We get a hardware timestamp on the NTP request packet so there is zero offset there.
//
// When building a reply packet, at some point we must read the
// clock (timer value) to create the transmit timestamp. However, it will take
// some time after reading the clock before we can transmit the
// reply:
//
// 1. We must convert the timer value into NTP time format
// 2. Copy the timestamp into the packet structure.
// 3. Load the packet data into the W5500.
// 4. Kick off packet transmission in the W5500.
//
// As a result if we use the timer value for the transmit
// time stamp it will be in error by the time required to
// execute steps (1-4) above. Here's how we solve that:
//
// a) We choose some time interval that is slightly longer than steps (1-4) 
//    will ever take (e.g. 800us). 
//
// b) We add this much time (e.g. 800us) to the transmit timestamp 
//    in the reply packet.
//
// c) after completing step (4), wait for the time interval (e.g. 800us)
//    to expire. This is done by comparing the current timer value to 
//    that obtained in step (1).
//
// d) Send the short begin-transmission command to the W5500.
//
// This almost does the trick. However, there is still some time required
// for the SPI bus to send the transmit command to the W5500, and some more time
// before the W5500 actually begins transmission. We've estimated this
// extra time to be about 22us. This extra time must also be added to the transmit
// time stamp.
//
// One final note: if these values were to exceed about 11ms or so, the
// OFFSET_NS_2_NTP() macro above will not work and will need to be modified.
// measurements with Mega2560 board and 10MHz clock show about 610us required
// to get ready for transmit.
//
#define NTP_TX_DELAY_US          800U       /* how long after getting current time will we kick off packet transmission? */
#define NTP_TX_DELAY_TICKS       1600U       /* in 2MHz timer ticks */
//
// Transmit timestamp calibration is achieved through these macros. 
// Start by enabling the print statement in NtpServer.cpp which prints the difference between
// receive and transmit timestamps. Then measure the time between falling edges of the W5500's
// interrupt line -- this is the time between Rx and Tx events and it should match the printed
// timestamp difference. Adjust the values below until there is agreement.
//
// =============== 16MHz Mega1280 Measurements (SPI at F_CPU/4)  =========================
//
// Time from call to endPacket and SEND_OK interrupt is about 22us.
// fine tuning with a scope resulting in the final value of 25,600ns.
//
#define NTP_TX_FINE_ADJUST_NS   25600UL
#define NTP_TX_ADJUST_NS        (1000UL * NTP_TX_DELAY_US + NTP_TX_FINE_ADJUST_NS)
#define NTP_TX_ADJUST_NTP       (OFFSET_NS_2_NTP(NTP_TX_ADJUST_NS))

#endif
