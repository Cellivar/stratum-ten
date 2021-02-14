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
// ========================================================================
// 
// Credits:
// 
// The author of this software wishes to thank Andrew Rodland of Madison, WI
// for ideas presented on the following internet web page:
//     http://cleverdomain.org/yapc2012/
// and in software made available at the following URL:
//     http://git.cleverdomain.org/arduino/
// 
// =========================================================================
// 
// 

#pragma once

#include "Timekeeping.h"
#include "FractionalN.h"

class NtpFractionalN : public Timekeeping, public FractionalN
{
public:

    NtpFractionalN();

    ~NtpFractionalN();

    //
    // these could be used during operation to fine-tune tx and rx offsets,
    // but for now they are unused.
    //
    void SetCaptureOffsetNtp(int32_t Offset);
    void SetCurrentOffsetNtp(int32_t Offset);

    uint8_t UnservicedInterruptCount();
    //
    // forces the current NTP time in seconds to the indicated value
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
    uint32_t GetTimeNs(uint16_t TimerCount);
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
    //
    // This overrides the base class ISR because we need to make a copy of fractional-N 
    // state if there has been an un-handled timer capture from the W5500 interrupt pin
    // when the ISR runs. This also calls the base class ISR to finish processing the interrupt.
    //
    void TimerMatchIsr();
    void PpsCaptureIsr();

protected:
    
    void NtpTime(pFracNState state, int32_t Offset, uint32_t *Seconds, uint32_t *Fraction);
    
    uint8_t ntpSecondsValid;
    uint32_t ntpSecondsOffset;          // added to our seconds count to make true NTP seconds.
    int32_t  ntpCurrentTimeOffset;      // fudge factor set by client to make results accurate
    int32_t  ntpCapturedTimeOffset;     // fudge factor set by client...
    //
    // we track client adjustments separately from internal adjustments
    // a typical client usage is accounting for antenna cable delays.
    //
    int32_t  clientCurrentTimeOffset = 0UL;
    int32_t  clientCapturedTimeOffset = 0UL;
    //
    // values captured when our ISR runs if there is a pending UDP packet time capture
    //
    volatile FracNState udpCapture;
};
