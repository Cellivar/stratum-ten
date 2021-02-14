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
#include <HardwareSerial.h>
#include "NtpTime.h"

class SerialGps
{

public:

    SerialGps(HardwareSerial* ComPort) : ntpTime()
    {
        port = ComPort;
    }
    
    virtual void Init() { Serial.println("Default Init for Serial GPS called. This is an error."); }
    virtual void ResetTodRequest() { }
    virtual void FlushInput() { }
    virtual uint8_t IoInProcess() { return 0U; }
    //
    // runs a state machine, querying different interesting data from GPS
    // hardware, such as current TOD and status such as locked/holdover/sat info/etc.
    //
    virtual uint8_t Step() { }
    // 
    // gets the last time of day acquired from the GPS receiver.
    // if zero is returned, it means that there is no new time of day available 
    // since the last time of day was acquired. For example, calling this function
    // twice in a row with no delay will normally result in the second call returning zero.
    //
    virtual uint32_t GetTimeOfDay() 
    { 
        uint32_t t = currentTime; 
        currentTime = 0UL; 
        return t; 
    }

    virtual uint8_t Locked() { return locked; }
    virtual uint8_t Holdover() { return holdover; }
    virtual uint8_t PpsValid() { return ppsValid; }
    virtual uint8_t LeapSecondPending() { return leapSecondPending; }
    virtual uint8_t AllConditionsNormal() { return allConditionsNormal; }
protected:
    
    HardwareSerial *port;
    NtpTime ntpTime;
    uint32_t currentTime = 0UL; // in NTP seconds
    uint8_t locked = 1U;
    uint8_t holdover = 0U;
    uint8_t ppsValid = 1U;
    uint8_t allConditionsNormal = 1U;
    uint8_t leapSecondPending = 0U;
};
