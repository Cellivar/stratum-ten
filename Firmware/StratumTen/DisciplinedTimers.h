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

#include <inttypes.h>
#include "Timekeeping.h"

class DisciplinedTimers : public Timekeeping
{
public:
    DisciplinedTimers();
    void TimerMatchIsr();
    void PpsCaptureIsr();

    uint8_t UnservicedInterruptCount();
    uint8_t PpsOccurred();
    //
    // these could be used during operation to fine-tune tx and rx offsets,
    // but for now they are unused.
    //
    void SetCaptureOffsetNtp(int32_t Offset);
    void SetCurrentOffsetNtp(int32_t Offset);
    //
    // forces the current NTP time in seconds to the indicated value.
    // DO NOT call this function during the last half of the last timer cycle 
    // This is typically called after I/O w/ref clock provides a value for NTP time.
    // The main loop should restrict I/O with ref clock to central timer cycles only
    // to meet this requirement.
    //
    uint8_t SetNtpSeconds(uint32_t Seconds);
    uint8_t NtpSecondsValid();
    //
    // returns the current time in integer NTP seconds.
    //
    uint32_t GetNtpSeconds();
    //
    // This must be called with a TimerCount value for the current timer cycle
    // If any timer match interrupts have occurred after acquiring TimerCount
    // and before calling this function, the results will not be valid.
    //
    //uint32_t GetTimeNs(uint16_t TimerCount);
    //
    // Specifically intended to obtain a transmit timestamp for reply to an NTP request.
    // As such it adds in pre-configured adjustments to account for transmit
    // packet formatting delays and results in an accurate timestamp. These adjustments
    // are set in the constructor using values imported from NtpConfig.h
    //
    // The adjustments are designed to work with timer-generated software delays 
    // which are implemented in the NtpServer class. These adjustments rely on the
    // proper delays being implemented in NtpServer.
    //
    void GetCurrentNtpTime(uint16_t *TimerCount, uint32_t *Seconds, uint32_t *Fraction);
    //
    // Resets flags associated with UDP packet time captures
    //
    void ResetCapturedNtpTime();
    //
    // Specifically intended to obtain a receive timestamp for reply to an NTP request.
    // As such it adds in pre-configured adjustments to account for time offset between
    // actual packet receipt and the associated hardware interrupt. These adjustments
    // are set in the constructor using values imported from NtpConfig.h
    //
    // returns false (zero) if there is no input capture event
    //
    uint8_t GetCapturedNtpTime(uint32_t *Seconds, uint32_t *Fraction);

private:
    void NtpTime(uint32_t NtpSeconds, uint16_t TimerCount, uint8_t TimerCycle, int32_t NtpFracOffset, uint32_t *Seconds, uint32_t *Fraction);

    volatile uint16_t timerPpsValue;
    volatile uint8_t  subCycleCount;
    volatile uint8_t  timersInitialized;
    
    volatile uint16_t  interruptCount;
    volatile uint8_t  ppsCaptureFlag;

    volatile uint8_t  udpCaptureFlag;
    volatile uint16_t udpCaptureCount;
    volatile uint8_t  udpCaptureCycle;
    volatile uint32_t udpCaptureSeconds;    // ntp seconds at capture
    
    uint32_t ntpSecondsValid;
    volatile uint32_t ntpSeconds;   // our current value of integer NTP time
    //
    // we track client adjustments separately from internal adjustments
    // a typical client usage is accounting for antenna cable delays.
    //
    int32_t  clientCurrentTimeOffset;
    int32_t  clientCapturedTimeOffset;
    //
    // these values will only contain internally generated offsets which are required
    // to make ntp time values accurate
    //
    int32_t  ntpCurrentTimeOffset; 
    int32_t  ntpCapturedTimeOffset;
    //
    // fractional NTP time values corresponding to the start of each of 160 timer cycles which
    // comprise a full second of time. tabulated here to save time having to compute them every time
    // the tradeoff is that it adds 640 bytes to program memory and RAM usage, which is acceptable 
    // for the Arduino Mega.
    //
    uint32_t ntpCycleTimes[NOM_CYCLE_PER_SEC] =
    {
        0x00000000, 0x0199999A, 0x03333333, 0x04CCCCCD, 0x06666666, 0x08000000, 0x0999999A, 0x0B333333, 
        0x0CCCCCCD, 0x0E666666, 0x10000000, 0x1199999A, 0x13333333, 0x14CCCCCD, 0x16666666, 0x18000000, 
        0x1999999A, 0x1B333333, 0x1CCCCCCD, 0x1E666666, 0x20000000, 0x2199999A, 0x23333333, 0x24CCCCCD, 
        0x26666666, 0x28000000, 0x2999999A, 0x2B333333, 0x2CCCCCCD, 0x2E666666, 0x30000000, 0x3199999A, 
        0x33333333, 0x34CCCCCD, 0x36666666, 0x38000000, 0x3999999A, 0x3B333333, 0x3CCCCCCD, 0x3E666666, 
        0x40000000, 0x4199999A, 0x43333333, 0x44CCCCCD, 0x46666666, 0x48000000, 0x4999999A, 0x4B333333, 
        0x4CCCCCCD, 0x4E666666, 0x50000000, 0x5199999A, 0x53333333, 0x54CCCCCD, 0x56666666, 0x58000000, 
        0x5999999A, 0x5B333333, 0x5CCCCCCD, 0x5E666666, 0x60000000, 0x6199999A, 0x63333333, 0x64CCCCCD, 
        0x66666666, 0x68000000, 0x6999999A, 0x6B333333, 0x6CCCCCCD, 0x6E666666, 0x70000000, 0x7199999A, 
        0x73333333, 0x74CCCCCD, 0x76666666, 0x78000000, 0x7999999A, 0x7B333333, 0x7CCCCCCD, 0x7E666666, 
        0x80000000, 0x8199999A, 0x83333333, 0x84CCCCCD, 0x86666666, 0x88000000, 0x8999999A, 0x8B333333, 
        0x8CCCCCCD, 0x8E666666, 0x90000000, 0x9199999A, 0x93333333, 0x94CCCCCD, 0x96666666, 0x98000000, 
        0x9999999A, 0x9B333333, 0x9CCCCCCD, 0x9E666666, 0xA0000000, 0xA199999A, 0xA3333333, 0xA4CCCCCD, 
        0xA6666666, 0xA8000000, 0xA999999A, 0xAB333333, 0xACCCCCCD, 0xAE666666, 0xB0000000, 0xB199999A, 
        0xB3333333, 0xB4CCCCCD, 0xB6666666, 0xB8000000, 0xB999999A, 0xBB333333, 0xBCCCCCCD, 0xBE666666, 
        0xC0000000, 0xC199999A, 0xC3333333, 0xC4CCCCCD, 0xC6666666, 0xC8000000, 0xC999999A, 0xCB333333, 
        0xCCCCCCCD, 0xCE666666, 0xD0000000, 0xD199999A, 0xD3333333, 0xD4CCCCCD, 0xD6666666, 0xD8000000, 
        0xD999999A, 0xDB333333, 0xDCCCCCCD, 0xDE666666, 0xE0000000, 0xE199999A, 0xE3333333, 0xE4CCCCCD, 
        0xE6666666, 0xE8000000, 0xE999999A, 0xEB333333, 0xECCCCCCD, 0xEE666666, 0xF0000000, 0xF199999A, 
        0xF3333333, 0xF4CCCCCD, 0xF6666666, 0xF8000000, 0xF999999A, 0xFB333333, 0xFCCCCCCD, 0xFE666666
    };

};

#endif
