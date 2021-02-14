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

#include "NtpHdwr.h"
#include "NtpConfig.h"

//
// gets everything ready to run but does not enable 
// the various timer interrupts (but timers are started running)
//
void InitializeHardware()
{
    pinMode(48, INPUT);   // ICR5 connected to W5500 interrupt pin
    pinMode(49, INPUT);   // ICR4 connected to 1 PPS signal
    //
    // pins 7,8,13 are used by the ethernet shield...???
    //
    pinMode(13, OUTPUT);
    digitalWrite(13,LOW);

    pinMode(7, OUTPUT);  
    digitalWrite(7, LOW);

    pinMode(8, OUTPUT);
    digitalWrite(8, LOW);
    //
    // pins for Serial port to USB interface
    //
    pinMode(19,INPUT);
    pinMode(18,OUTPUT);
    digitalWrite(18,HIGH);

    // PORTH &= ~_BV(PORTH4); // ???
    
    //
    // reset and hold timer prescalers
    //
    GTCCR = _BV(TSM);
    GTCCR = _BV(TSM) | _BV(PSRASY) | _BV(PSRSYNC);
    //
    // configure timers 4 and 5
    // both are setup identically except input capture on T4 is rising
    // edge for PPS and falling edge on T5 for W5500 interrupt line.
    //
    TCCR4A = 0;
    TCCR5A = 0;  
    TIMSK4 = 0;     // initially, all interrupts disabled
    TIMSK5 = 0;     // we never use interrupts with T5
    //
    // ICESn selects rising edge input capture for the pin, leaving it zero selects falling edge
    // overall WGM is 0100 = 4 which selects CTC counting mode with TOP = OC4A,
    // and TOV4 flag set at MAX count. CTC clears timer on compare match where
    // the compare is with OCR4A
    //
    // The clock select bits are 001 = 0x01 for no clk prescaling (10MHz timer clock clock)
    // For 16MHz system clock, bits are 010 for divide by 8 prescaling
    //
#if SYSCLK_DISCIPLINED
    TCCR4B = _BV(CS40) | _BV(WGM42) | _BV(ICES4); // CTC mode, Clk/1 input capture rising edge of PPS.
    TCCR5B = _BV(CS50) | _BV(WGM52);   // CTC mode, Clk/1, input capture on falling edge of W5500 interrupt.
#else
    TCCR4B = _BV(CS41) | _BV(WGM42) | _BV(ICES4); // CTC mode, Clk/8 input capture rising edge of PPS.
    TCCR5B = _BV(CS51) | _BV(WGM52);   // CTC mode, Clk/8, input capture on falling edge of W5500 interrupt.
#endif
    //
    // preset timers to zero; they should count in sync forever from here
    // as long as we manage the output compare register values identically 
    // and carefully.
    //    
    TCNT4 = 0;
    TCNT5 = 0;   
    //
    // set timer match compare registers to the same default value
    //
    OCR4A = NOMINAL_INTEGER_DIVISOR - 1U;
    OCR5A = NOMINAL_INTEGER_DIVISOR - 1U;
    //
    // release reset on prescalers in case the Arduino platform needs timers running...
    //
    GTCCR = 0;
}
//
// clears any pending timer interrupts and enables them
//
void StartTimers(uint16_t Count)
{
    //
    // reset and hold timer prescalers
    //
    GTCCR = _BV(TSM);
    GTCCR = _BV(TSM) | _BV(PSRASY) | _BV(PSRSYNC);
    //
    // set timer counts to the same value
    //
    TCNT4 = Count;
    TCNT5 = Count;
    //
    // clear any pending interrupts, then enable input capture and output compare interrupts on timer 4
    //
    TIFR4  = _BV(OCF4A) | _BV(ICF4);
    TIMSK4 = _BV(OCIE4A) | _BV(ICIE4); 

    GTCCR = 0; // release reset on timer prescalers and let them start counting.
}
