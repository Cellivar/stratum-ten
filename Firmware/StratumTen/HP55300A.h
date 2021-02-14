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

#include "NtpConfig.h"

#if GPS_IS_HP55300A

#include <Arduino.h>
#include <inttypes.h>
#include <HardwareSerial.h>

#include "SerialGps.h"
#include "NtpTime.h"

class HP55300A : public SerialGps
{

public:

    HP55300A(HardwareSerial* ComPort);
    
    void Init();
    void ResetTodRequest();
    void FlushInput();
    uint8_t IoInProcess();

    // 
    // gets time of day from the HP55300A's time-of-day serial port.
    // write your own function using this as a template if you want to get
    // same from a GPS module instead.
    //
    // It's important to understand how the HP unit responds to TOD requests.
    // If a request is sent in between two PPS signals (which is just what we do),
    // then it will wait until after the next PPS signal to reply. That means it could
    // take up to one second to get the answer back. When we do get the reply, the time
    // encoded therein is the time of the NEXT one-second pulse, so the current time is
    // the reply time minus one second. 
    //
    // Arduino's hardware serial classes are interrupt driven (both for tx and rx) so it takes very little
    // time to load a query into Serial1's transmit buffer and the receive
    // buffer is large enough to hold the entire response; we just need to grab chars as
    // they show up and assemble into complete messages (delimited by new line chars).
    //
    uint8_t Step();

protected:
    
    void requestTimeOfDay();
    int getTodReply();
        
    uint8_t ioInProcess = 0U;
    char todBuffer[80];
    char todBufPtr = 0;
    uint8_t todOnRequest = 0;
    uint32_t queryTimeout;
    uint32_t nextQuery = 0U;
    
    const uint32_t QUERY_TIMEOUT = 3000UL;
    const uint32_t QUERY_INTERVAL = 74959UL;

    char *hexDigits = "0123456789ABCDEF";
};

#endif
