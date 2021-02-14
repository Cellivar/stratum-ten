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

#include "HP55300A.h"
#include "NtpHdwr.h"

#if GPS_IS_HP55300A

HP55300A::HP55300A(HardwareSerial *ComPort) : SerialGps(ComPort)
{
}

void HP55300A::Init()
{
}

uint8_t HP55300A::IoInProcess()
{
    return ioInProcess;
}

void HP55300A::ResetTodRequest()
{
    todOnRequest = 0;
    todBufPtr = 0;
}

void HP55300A::FlushInput()
{
    int len;
    //Serial.println("Flushing HP55300A serial port...");

    ResetTodRequest();
    port->print("*cls\n");
    uint32_t timeout = millis() + 1000L;
    while ((len = getTodReply()) || (millis() < timeout)) 
    {
        // if (len > 0) Serial.println(todBuffer);
        ResetTodRequest();
    }

    ResetTodRequest();
    port->print("*err?\n");
    timeout = millis() + 2000L;
    while ((len = getTodReply()) || (millis() < timeout)) 
    {
        // if (len > 0) Serial.println(todBuffer);
        ResetTodRequest();
    }

    ResetTodRequest();
}

void HP55300A::requestTimeOfDay()
{
    const char *todQuery = ":ptim:tcod?\n";
    port->print(todQuery);
    // Serial.println("Asked");
}

int HP55300A::getTodReply()
{
    //
    // this should not really happen, but just in case
    //
    if (todBufPtr >= sizeof(todBuffer))
    {
        todBuffer[sizeof(todBuffer)-1] = 0;
        return sizeof(todBuffer) - 1;
    }

    int rc = 0;    // return code is zero or length of string read

    while (port->available() > 0 && todBufPtr < (sizeof(todBuffer) - 1))
    {
        char c = port->read();
        // Serial.print(c);
        if (!iscntrl(c)) todBuffer[todBufPtr++] = c;
        if (c == '\n') 
        {
            rc = todBufPtr;
            todBuffer[todBufPtr++] = 0;                
            break;
        }
    }

    if (todBufPtr >= (sizeof(todBuffer) - 1))
    {
        rc = sizeof(todBuffer) - 1;
        todBuffer[sizeof(todBuffer) - 1] = 0;
    }

    return rc;
}

uint8_t HP55300A::Step()
{   
    //
    // what state are we in? have we already requested TOD?
    //
    if (todOnRequest)
    {        
        if (TIMEDOUT(millis(), queryTimeout))
        {
            todOnRequest = 0U; // forces the query to be re-sent next time we're called.
            nextQuery = millis() + 3000UL;
        }
    }
    else
    {
        if (TIMEDOUT(millis(), nextQuery))
        {
            // 
            // just make the request and return zero; 
            // the client will call us again (and again...) as the reply comes back.
            //
            ioInProcess = 1U;
            requestTimeOfDay();
            todBufPtr = 0;
            todOnRequest = 1;       // sent request, now find echo of request 
            queryTimeout = millis() + QUERY_TIMEOUT;
            return 0U;
        }
    }

    const char *clearErrors = "*cls\n";

    int len;
    uint16_t year;
    uint8_t month, day, hr, min, sec;
    byte err = 0;
    byte ok = 0;
    char *reply = todBuffer; 
    //
    // grab whatever data is available and look for a line-feed to terminate 
    // the reply. repeat until no more data is available.
    // for each line of reply, parse the data and process accordingly.
    //
    while ((len = getTodReply()) > 0)
    {
        // Serial.print("GPS["); Serial.print(len); Serial.print("]:"); Serial.println(reply);
        
        if (len == 23 && reply[0] == 'T' && reply[1] == '2')
        {
            // 
            // clear the TOD request since we have received a "T2" reply. 
            // if the reply is bad, we'll make another request when called again.
            // this will be transparent to the user...they'll just have to call us more times
            // before getting a non-zero return code.
            //
            todOnRequest = 0;
            //
            // verify the checksum
            //
            int sum = 0;
            for (int k=0; k<(len-2); sum += reply[k++]);
            char msn = hexDigits[(sum & 0xf0) >> 4];
            char lsn = hexDigits[sum & 0x0f];
            if (msn != reply[len-2] || lsn != reply[len-1]) continue;
            //
            // extract date and time
            //
            year = 1000 * (reply[2] & 0x0f) + 100 * (reply[3] & 0x0f) + 10 * (reply[4] & 0x0f) + (reply[5] & 0x0f);
            month = 10 * (reply[6] & 0x0f) + (reply[7] & 0x0f);
            day =  10 * (reply[8] & 0x0f) + (reply[9] & 0x0f);
            hr =  10 * (reply[10] & 0x0f) + (reply[11] & 0x0f);
            min =  10 * (reply[12] & 0x0f) + (reply[13] & 0x0f);
            sec =  10 * (reply[14] & 0x0f) + (reply[15] & 0x0f);
            //
            // get status flags
            //
            byte tfom = reply[16] & 0x0f;
            byte ffom = reply[17] & 0x0f;
            char leap = reply[18];

            switch(leap)
            {
            case '0':
                leapSecondPending = 0U;
                break;
            case '+':
                leapSecondPending = 1U; // time jumps forwards
                break;
            case '-':
                leapSecondPending = 0xffU; // time jumps backwards
                break;
            default:
                break;
            }

            byte valid = reply[19] == '0';
            // Serial.print("FFOM "); Serial.println(ffom);
            ok = valid && (ffom < 3);
            ioInProcess = 0U;

            nextQuery = millis() + (ok ? QUERY_INTERVAL : 3000UL);
        }
        if (strncmp(reply, "scpi >", 6) == 0)
        {
            // just an echo of the prompt and a TOD query...ignore it.
        }
        if (strncmp(reply, "E-", 2) == 0)
        {
            err = 1; // we should send a *CLS to clear the error...don't care what it is for now
        }

        todBufPtr = 0;  // clear the buffer point to receive another ascii message line
    }

    if (err) port->print(clearErrors);
    //
    // did we get a valid TOD?
    //
    if (ok) 
    {
        uint32_t ntpSecs = ntpTime.CalendarToNtpSeconds(year, month, day, hr, min, sec);
        //
        // ntpSecs is the time at the next PPS pulse. we want the current ntp second, so subtract one.
        //
        ntpSecs--;
        currentTime = ntpSecs;
        holdover = 0U;
        // Serial.print("NTP Time "); Serial.println(currentTime);

#if 0
        Serial.print(year); Serial.print('-');
        Serial.print(month); Serial.print('-');
        Serial.print(day); Serial.print(' ');
        Serial.print(hr); Serial.print(':');
        Serial.print(min); Serial.print(':');
        Serial.println(sec);
#endif
        allConditionsNormal = 1U;
        return 1U;
    }

    return 0U;
}

#endif
