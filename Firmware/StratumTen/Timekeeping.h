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

class Timekeeping
{
public:
    //
    // these can be used for specifying cable delay for instance.
    // units are fractional NTP seconds -- multiply delay in seconds by 2^32 to get these values.
    // these routines DO NOT support long delays (e.g. tenths of a second or more)
    //
    virtual void SetCaptureOffsetNtp(int32_t Offset) { };
    virtual void SetCurrentOffsetNtp(int32_t Offset) { };
    //
    // forces the current NTP time in seconds to the indicated value
    //
    virtual uint8_t SetNtpSeconds(uint32_t Seconds) { return 0U; }
    virtual uint8_t NtpSecondsValid() { return 0U; }  // returns zero if NTP seconds have never been set.
    //
    // returns the current time in integer NTP seconds.
    //
    virtual uint32_t GetNtpSeconds() { return 0UL; }
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
    virtual void GetCurrentNtpTime(uint16_t *TimerCount, uint32_t *Seconds, uint32_t *Fraction) { };
    //
    // Resets flags associated with UDP packet time captures
    //
    virtual void ResetCapturedNtpTime() { };
    //
    // Specifically intended to obtain a receive timestamp for reply to an NTP request.
    // As such it adds in pre-configured adjustments to account for time offset between
    // actual packet receipt and the associated hardware interrupt. These adjustments
    // are set in the constructor using values imported from NtpConfig.h
    //
    // returns false (zero) if there is no input capture event
    //
    virtual uint8_t GetCapturedNtpTime(uint32_t *Seconds, uint32_t *Fraction) { return 0U; }

    virtual uint8_t UnservicedInterruptCount() 
    { 
        Serial.println("ERROR: UnservicedInterruptCount in Timekeeping class called."); 
        return 0U; 
    }
};
