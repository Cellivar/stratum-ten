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
#include <Arduino.h>
#include "NtpConfig.h"
#include "NtpHdwr.h"
#include "DisciplinedTimers.h"

#if SYSCLK_DISCIPLINED

DisciplinedTimers::DisciplinedTimers()
{
    subCycleCount = 0U;
    timersInitialized = 0U;
    udpCaptureFlag = 0U;
    clientCapturedTimeOffset = 0UL;
    clientCurrentTimeOffset = 0UL;
    ntpCapturedTimeOffset = (NOM_CYCLE_NTP >> 1);
    ntpCurrentTimeOffset = ntpCapturedTimeOffset;
    ntpSeconds = 0UL;
    ntpSecondsValid = 0U;
    ppsCaptureFlag = 0U;
}

void DisciplinedTimers::TimerMatchIsr()
{
    if (!timersInitialized) return;
    //
    // if there's a pending UDP packet capture (ICR5) then save that data first
    //
    if (UDP_CAPTURE_FLAG) 
    {
        udpCaptureSeconds = ntpSeconds;
        udpCaptureCycle = subCycleCount;
        udpCaptureCount = UDP_CAPTURE_COUNT;
        udpCaptureFlag |= 0x1U;
        RESET_UDP_CAPTURE_FLAG;
    }

    if (subCycleCount >= (NOM_CYCLE_PER_SEC-1U))
    {
        subCycleCount = 0U;
        ntpSeconds++;
    }
    else
    {
        subCycleCount++;
    }
    interruptCount++;
}

void DisciplinedTimers::PpsCaptureIsr()
{
    if (timersInitialized > 1U) // check for the most frequent situation first
    {
        uint16_t pps = PPS_CAPTURE_COUNT;
        //
        // eventually this limit s/b dropped to either +-1 count or an exact match
        // depending on the phase relationship between 10MHz system clock and the PPS 
        // signal, including internal delays within the ATMEGA processor.
        //
        if (pps < (timerPpsValue - 1U) || pps > (timerPpsValue + 1U))
        {
            timersInitialized = 0U;
            return;
        }
        ppsCaptureFlag = 1U;
        return;
    }

    if (timersInitialized == 0U)
    {        
        SET_TIMERS_COUNT( PPS_TIMER_PRESET );         // this is a macro defined in NtpHdwr.h
        subCycleCount = NOM_CYCLE_PER_SEC - 1U;
        timersInitialized = 1U;
        return;
    }
    //
    // timersInitialized can only be equal to 1U here
    //
    timerPpsValue = PPS_CAPTURE_COUNT;
    int16_t delta = timerPpsValue - (NOMINAL_INTEGER_DIVISOR >> 1);
    //
    // the timer should be half-way through the cycle +-20us 
    //
    if ((delta < -200) || (delta > 200))
    {
        timersInitialized = 0U;
        return;
    }
    //
    // this offset is the value which, when added to "timerPpsValue" is exactly equal to
    // a full timer cycle in NS. A full timer cycle in ns is "NOMINAL_INTEGER_DIVISOR" counts
    // multiplied by the duration of a timer tick in ns.
    //
    uint32_t offsetNs = NOM_TICK_NS * (NOMINAL_INTEGER_DIVISOR - timerPpsValue);
    // 
    // convert offset in ns to offset in NTP fractional seconds
    //
    uint32_t offsetNtp = OFFSET_NS_2_NTP(offsetNs);

    ntpCapturedTimeOffset = offsetNtp;
    ntpCurrentTimeOffset = offsetNtp;
    timersInitialized = 2U;
}

uint8_t DisciplinedTimers::PpsOccurred()
{
    uint8_t rc = ppsCaptureFlag;
    ppsCaptureFlag = 0U;
    return rc;
}

uint8_t DisciplinedTimers::UnservicedInterruptCount()
{
    if (interruptCount == 0U) return 0U;
    uint8_t cnt = (interruptCount > 0xff) ? 0xff : (uint8_t)interruptCount;
    interruptCount--;
    return cnt;
}

void DisciplinedTimers::SetCaptureOffsetNtp(int32_t Offset) 
{
    clientCapturedTimeOffset = Offset;
}

void DisciplinedTimers::SetCurrentOffsetNtp(int32_t Offset) 
{
    clientCurrentTimeOffset = Offset;
}

uint8_t DisciplinedTimers::SetNtpSeconds(uint32_t Seconds)
{
    uint32_t prevSeconds = ntpSeconds;
    uint8_t prevValid = ntpSecondsValid;

    ntpSeconds = Seconds;
    ntpSecondsValid = 1U;
    
    if (!prevValid || (ntpSeconds != prevSeconds))
    {
        //
        // print a message if we changed the value.
        //
        Serial.print("! Set NTP sec = "); Serial.println(ntpSeconds); 
    }
    //
    // return of false means that we were tracking ntp seconds and had to change
    // the tracking based on new input (we'd lost or gained one or more seconds).
    //
    return (!prevValid) || (ntpSeconds == prevSeconds);
}

uint8_t DisciplinedTimers::NtpSecondsValid() 
{
    return ntpSecondsValid;
}

uint32_t DisciplinedTimers::GetNtpSeconds()
{
    return ntpSeconds;
}

void DisciplinedTimers::GetCurrentNtpTime(uint16_t *TimerCount, uint32_t *Seconds, uint32_t *Fraction)
{
    //
    // deal with the possibility of a timer match interrupt occuring during our reading of the current time variables
    // with some versions, the timer runs on system clock so we won't be able to read the same timer value twice,
    // but we should be able to get the same sub-cycle count twice in row.
    // this method is typically only used to get NTP transmit time stamps. 
    // receive timestamps are generated by timer input capture events from the NIC chip's interrupt signal.
    //
    uint16_t t2;
    uint32_t seconds2;
    uint8_t cycle2;
    uint8_t changed;
    uint16_t t = FRAC_N_COUNT;
    uint32_t seconds = ntpSeconds;
    uint8_t cycle = subCycleCount;
    //
    // current time offset includes adjustement for the pps interrupt occuring half-way through the timer cycle.
    // client offset is for things like antenna cable delay
    //
    int32_t offset = ntpCurrentTimeOffset + clientCurrentTimeOffset;

    do
    {
#if ENABLE_TIMESTAMP_DEBUG_FLAG
    PORTH &= ~0x20; // turn off flag to indicate we're about to acquire time
    // if this is a 2nd or 3rd pass through the loop we are turning the flag off
    // after a short time (usec or two) so there will be a short pulse.
#endif
        t2 = FRAC_N_COUNT;
#if ENABLE_TIMESTAMP_DEBUG_FLAG
    PORTH |= 0x20; // turn on debug flag indicating we've captured current time
    // if things changed and the loop repeats there will be a short pulse followed by
    // the flag going on and staying on
#endif
        seconds2 = ntpSeconds;
        cycle2 = subCycleCount;
        
        changed = (cycle2 != cycle) || (seconds2 != seconds);

        seconds = seconds2;
        cycle = cycle2;
        t = t2;
    } while (changed);


    *TimerCount = t;
    
    NtpTime(seconds, t, cycle, offset, Seconds, Fraction);
}

uint8_t DisciplinedTimers::GetCapturedNtpTime(uint32_t *Seconds, uint32_t *Fraction)
{
    uint8_t ok = 0;

    if (udpCaptureFlag) // was this already detected by the timer match (i.e. overflow) ISR?
    {
        ok = 1;
    }
    else
    {
        ok = (UDP_CAPTURE_FLAG != 0);
        // 
        // what if a capture interrupt occurs here...? 
        // probably do nothing as we're already going to return false so the client is duly warned.
        // 
        udpCaptureSeconds = ntpSeconds;
        udpCaptureCycle = subCycleCount;
        udpCaptureCount = UDP_CAPTURE_COUNT;
    }
    
    udpCaptureFlag = 0U; //&= ~0x01U;
    RESET_UDP_CAPTURE_FLAG; 
    
    int32_t offset = ntpCapturedTimeOffset + clientCapturedTimeOffset;

    NtpTime(udpCaptureSeconds, udpCaptureCount, udpCaptureCycle, offset, Seconds, Fraction);

    return ok;
}

void DisciplinedTimers::NtpTime(uint32_t NtpSeconds, uint16_t TimerCount, uint8_t TimerCycle, int32_t NtpFracOffset, uint32_t *Seconds, uint32_t *Fraction)
{
    uint32_t timerNs = NOM_TICK_NS * (uint32_t)(TimerCount);
    uint32_t frac = OFFSET_NS_2_NTP(timerNs);
    frac += ntpCycleTimes[TimerCycle];
    //
    // this addition of Offset is typically around 1/2 NOM_CYCLE_NS and can roll-over in 32-bit math
    // if that happens we know because the sum will be less than the original value.
    // when rollover occurs, we increment the integer seconds count to account for it.
    //
    uint32_t offsetFrac;

    if (NtpFracOffset < 0UL) 
    {
        //
        // this will NOT usually happen...and in reality it might just work if we
        // compute "frac - NtpFracOffset" w/o any type casting, but this will always work.
        //
        offsetFrac = frac - (uint32_t)(-NtpFracOffset);
    }
    else
    {
        offsetFrac = frac + (uint32_t)NtpFracOffset;
    }

    uint32_t sec = NtpSeconds;
    if (offsetFrac < frac) sec++;
    
    *Seconds = sec;
    *Fraction = offsetFrac;
}

void DisciplinedTimers::ResetCapturedNtpTime()
{
    udpCaptureFlag &= ~0x01U;
    RESET_UDP_CAPTURE_FLAG;
}


#endif

