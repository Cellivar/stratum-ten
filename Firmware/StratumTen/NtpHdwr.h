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

#include <Arduino.h>
#include <inttypes.h>
#include "NtpConfig.h"

// Pin assignments:
//
// PL0 / digital 49 is ICP4 connected to 1PPS signal.
// PL1 / digital 48 is ICP5 and connected to W5500 interrupt
//
// PD2 / RXD1 / digital 19 is RS232 data in from GPS
// PD3 / TXD1 / digital 18 is RS232 data out to GPS
//
// We force T5 to track counts with T4. The W5500 is setup to 
// interrupt when data is received on the NTP port and that drives 
// input capture ICP5. Therefore we get a timer value accurate +-500ns
// of exactly when the packet arrived.
//
// standard connections to WizNet W5500-based ethernet shield v2 are assumed
// except the interrupt pin on the W5500 needs to be wired over to
// digital 48 as noted above.
//

//
// A note on philosophy: the tie between hardware and conceptual functions 
// such as setting the integer frac-N division ratio can either be encoded as a 
// function or a macro. This is an embedded system; we don't want to waste CPU cycles
// making function calls just to get or set a processor register value. 
// Yes, the compiler can theoretically implement these simple functions in-line
// but do we really want to depend on that and go to the trouble to verify it?
// Therefore, hardware dependence is encoded with macros which are guaranteed to be
// in-line.
//

#define FRAC_N_COUNT                TCNT4
#define SET_FRAC_N_INTERVAL(x)      OCR4A = (x)-1U; OCR5A = (x)-1U
#define FRAC_N_INTERVAL             (OCR4A+1U)
#define PPS_CAPTURE_COUNT           ICR4

#define UDP_CAPTURE_FLAG            (TIFR5 & _BV(ICF5))
#define RESET_UDP_CAPTURE_FLAG      TIFR5 = _BV(ICF5)
#define UDP_CAPTURE_COUNT           ICR5
#define UDP_CAPTURE_INTERVAL        (OCR5A+1U)
//
// to set both timers to the same value, the prescaler must be stopped and cleared while the changes are made.
//
#define SET_TIMERS_COUNT(x)         GTCCR = _BV(TSM); GTCCR = _BV(TSM) | _BV(PSRASY) | _BV(PSRSYNC); TCNT4 = x; TCNT5 = x; GTCCR = 0

//#define TIMER_MATCH_INTERRUPTS_OFF  TIMSK4 &= ~(_BV(OCIE4A))
//#define TIMER_MATCH_INTERRUPTS_ON   TIMSK4 |= _BV(OCIE4A)

#define TCCR0B_CS_MASK  ( _BV(CS02)| _BV(CS01) | _BV(CS00) )

#if EARLY_PROTOTYPE_LEDS

#define LED_ERROR           25      // red error indicator (flash on invalid NTP request?)
#define LED_TOD_OK          24      // green when TOD is valid
#define LED_PLL_LOCK        27      // green when PLL is locked
#define LED_PPS             26      // green flash when PPS is processed
#define LED_RQST            28      // green flash when NTP reply is sent

#else

#if SYSCLK_DISCIPLINED

#define LED_ERROR           28      // red error indicator (flash on invalid NTP request?)
#define LED_HOLDOVER        30      // green when TOD is valid
#define LED_GPS_LOCK        32      // green when PLL is locked
#define LED_PPS             34      // green flash when PPS is processed
#define LED_RQST            35      // green flash when NTP reply is sent

#else

#define LED_ERROR           28      // red error indicator (flash on invalid NTP request?)
#define LED_TOD_OK          30      // green when TOD is valid
#define LED_PLL_LOCK        32      // green when PLL is locked
#define LED_PPS             34      // green flash when PPS is processed
#define LED_RQST            35      // green flash when NTP reply is sent

#endif

#endif

//
// detecting timeout with uint32_t clock requires a check for overflow...
// e.g. if the timeout was set at time 0xffffffff to two ticks in the future, then
// timeout will be 0x00000001. here, (now > timeout) but has not yet expired so we must
// also check that the difference is less than a reasonable amount.
//
#define TIMEDOUT(now,timeout)       ((now >= timeout) && ((now - timeout) < 0x80000000UL))

extern void InitializeHardware();
extern void StartTimers(uint16_t Count);
