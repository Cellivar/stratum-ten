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
#include "NtpDebug.h"
#include "IcmSmt360.h"
#include "TrimbleTSIP.h"
#include "NtpHdwr.h"
#include "NtpTime.h"

#if SUPPORT_DOUBLES
// for conversion of double-precision floating point data to single precision
#include "IEEE754utils.h"    
#endif

#if GPS_IS_ICM_SMT_360

#define QUERY_TIMEOUT 2000UL  


#define STATE_SEND_INIT    1
#define STATE_RCV_INIT     2
#define STATE_RQST_PTI     3
#define STATE_RCV_PTI      4
#define STATE_RQST_STI     5
#define STATE_RCV_STI      6

IcmSmt360::IcmSmt360(HardwareSerial *ComPort) : SerialGps(ComPort)
{
    allConditionsNormal = 0U;
    requestState = STATE_RQST_PTI;
}

void IcmSmt360::write(uint8_t *msg, uint8_t count)
{
    uint8_t k;
    for (k=0; k<count; port->write(msg[k++]));
}

void IcmSmt360::Init()
{
    Init(0U);
}

void IcmSmt360::Init(uint8_t RequestSurvey)
{
    clientWantsSurvey = RequestSurvey;
    requestState = STATE_SEND_INIT;
    initIndex = 0U;
    nextQueryTime = millis();

    //write(msgBroadcastOff, sizeof(msgBroadcastOff));    // turn off all automatic broadcasts
    //write(msgUtcTiming, sizeof(msgUtcTiming));          // select UTC instead of GPS time
    //write(msgDiscipline, sizeof(msgDiscipline));        // turn off jam-sync on the PPS signal
}

void IcmSmt360::ResetInputBuffer()
{
    todBufPtr = -1;
    som = 0;
}

void IcmSmt360::FlushInput()
{
    int len;

    ResetInputBuffer();

    uint32_t timeout = millis() + 500UL;
    while ((len = getReply()) || (millis() < timeout)) 
    {
        ResetInputBuffer();
    }

    ResetInputBuffer();
}

uint8_t IcmSmt360::IoInProcess()
{
    return ioInProcess;
}

uint8_t IcmSmt360::Locked()
{
    return locked;
}

uint8_t IcmSmt360::Holdover()
{
    return holdover;
}
//
// returns zero if there is no complete message available
// keep calling until a non-zero return indicates the size of
// complete message. Message header (0x10), trailer (0x10, 0x03)
// and any repeated 0x10 chars have been removed by this function.
//
// This function wil try to get data for at most 500us and will return if there is not a complete
// message available within that time. Next call will pick up with previous data.
// User must call ResetInputBuffer() after a complete message before calling this again to get next msg.
//
int IcmSmt360::getReply()
{
    uint8_t c;
    int rc = 0;    // return code is zero or count of bytes read
    uint32_t timeout = micros() + 500UL;

    while (!rc && todBufPtr < (int8_t)sizeof(todBuffer))
    {
        if (TIMEDOUT(micros(), timeout)) break;
        
        if (port->available())
        {
            c = (uint8_t)port->read();
        }
        else
        {
            continue;
        }
        //
        // we require an SOM char to begin a new message
        //
        if (todBufPtr < 0)
        {
            if (c != SOM) continue;  // into the bit bucket with you!
            todBufPtr = 0; // valid start of message
            continue;
        }

        todBuffer[todBufPtr++] = c; // add char to buffer

        switch(c)
        {
        case 0x10:  // start-of-message value
            som++;
            //
            // remove this char if it is a repeated SOM
            //
            if ((todBufPtr > 1) && (todBuffer[todBufPtr-2] == SOM)) todBufPtr--;
            break;
        case 0x03:  // end-of-message value
            // 
            // it's only a valid end marker if the previous count of SOM chars is odd
            //
            if ((som & 0x01) == 1) 
            {
                rc = todBufPtr;
            }
            break;
        default:
            break;
        }
    }

    if (todBufPtr >= (int8_t)sizeof(todBuffer))
    {
        //
        // buffer overflow...just throw out the data
        //
        return -1;
    }

    if (rc)
    {
        //
        // buffer now contains the message without the beginning SOM header but does include the SOM/ETX trailer 
        // remove the trailer by decrementing the message length by two.
        //
        if (rc < 3)
        {
            // something's wrong; just clear the buffer
            rc = 0;
            ResetInputBuffer();
            return rc;
        }

        rc -= 2;
    }
    // if (rc > 0)   { Serial.print("read "); Serial.println(rc); }

    return rc;
}
//
// all TSIP multi-byte data is big-endian; these functions will convert it to
// little endian as required by Atmel AVR processors.
//
uint32_t IcmSmt360::buf2uint32(uint8_t *buf)
{
    uint32_t x = *buf++;
    for (int k=0; k<3; k++) x = (x << 8) | *buf++;
    return x;
}

uint16_t IcmSmt360::buf2uint16(uint8_t *buf)
{
    uint16_t x = *buf++;
    return (x << 8) | *buf;
}

int16_t IcmSmt360::buf2int16(uint8_t *buf)
{
    uint16_t x = *buf++;
    x = (x << 8) | *buf;
    if (x & 0x8000U)
    {
        return (int16_t)x;
    }
    return (int16_t)x;
}

float IcmSmt360::buf2float(uint8_t *buf)
{
    uint32_t x = buf2uint32(buf); // get the indians fixed
    return *((float*) &x);
}

uint8_t IcmSmt360::parsePTI(int8_t length)
{
    if (length != 18) return 0U;
    //
    // make sure the receiver has UTC offset info and time units are set to UTC, not GPS time.
    //
    uint8_t utcOk = todBuffer[10] == 0x03U;
    if (utcOk != haveUtcOffset)
    {
        debug_str(utcOk ? "! UTC OK\r\n" : "! UTC lost\r\n");
    }
    haveUtcOffset = utcOk;
    updateStatus();

    if (!haveUtcOffset) return 1U;
    //
    // make sure there are no alarms or state info to indicate that time is invalid
    //
    if (disciplineMode == 1 || disciplineMode >5) return 1U;
    if (disciplineActivity != 0U && disciplineActivity != 8U) return 1U;
    if (disciplineMode == 2U || disciplineMode == 3U) 
    {
        // holdover is active
        if (holdoverDuration > maxHoldoverDuration) return 1U;
    }
    // 
    // the following alarms are killers: no PPS, questionable posn, test mode, not disciplining
    //
    if ((minorAlarms & 0x1310) != 0) return 1U;
    //
    // time reported is time at next PPS, so the current time is one second earlier than that.
    //
    uint16_t y = buf2uint16(todBuffer+16);
    uint32_t tNtp = ntpTime.CalendarToNtpSeconds(y, todBuffer[15], todBuffer[14], todBuffer[13], todBuffer[12], todBuffer[11]);
    //
    // unlike the HP55300A, time from this GPS receiver is the current second, not the next second
    // and we don't need to subtract one from it.
    //
    // ************************ WARNING WARNING WARNING **************************
    // this code was working just fine, then at some point it started reporting
    // time that was one second too small. there is no clue as to why
    // this happened.
    // The fix (for now) is to add one second to the reported time.
    // for future reference, errors in time of exact multiples of one second may
    // be caused by this line of code.
    // ***************************************************************************
    //
    currentTime = tNtp + 1UL;

    return 1U;
}

uint8_t IcmSmt360::parseSTI(int8_t length)
{
    if (length != 69) return 0U;

    char *s;
    
    if (gpsStatus != todBuffer[13])
    {
        if (gpsStatus < (sizeof(gpsStatusNames)/sizeof(char*)))
        {
            s = gpsStatusNames[todBuffer[13]];
            if (s)
            {
                Serial.print("! Gps status ");
                Serial.println(s);
            }
        }
    }
    gpsStatus = todBuffer[13];

    if (disciplineMode != todBuffer[3]) 
    {
        if (todBuffer[3] < (sizeof(disciplineModeNames)/sizeof(char*)))
        {
            s = disciplineModeNames[todBuffer[3]];
            if (s)
            {
                Serial.print("! Discipline mode "); 
                Serial.println(s);
            }
        }
    }
    disciplineMode = todBuffer[3];
    
    if (disciplineActivity != todBuffer[14])
    {
        if (todBuffer[14] < (sizeof(disciplineActivityNames)/sizeof(char*)))
        {
            s = disciplineActivityNames[todBuffer[14]];
            if (s)
            {
                Serial.print("! Discipline activity ");
                Serial.println(s);
            }
        }
    }
    disciplineActivity = todBuffer[14];

    if (surveyProgress != todBuffer[4])
    {
        if (todBuffer[4] == 100) surveyPosValid = 0U; // force update of position variables
        Serial.print("! S");
        Serial.println(todBuffer[4]);
    }
    surveyProgress = todBuffer[4];

#if SUPPORT_DOUBLES
    if (!surveyPosValid)
    {
        //
        // grabs either the stored survey position or the new updated one
        //
        surveyLatitude = doublePacked2Float(todBuffer+37, MSBFIRST) * DEG_PER_RAD;
        surveyLongitude = doublePacked2Float(todBuffer+45, MSBFIRST) * DEG_PER_RAD;
        surveyAltitude = doublePacked2Float(todBuffer+53, MSBFIRST);
        surveyPosValid = 1U;
        Serial.print("! Position "); Serial.print(surveyLatitude, 5);
        Serial.print(", "); Serial.print(surveyLongitude, 5);
        Serial.print(", "); Serial.print(surveyAltitude, 2);
        Serial.println();
    }
#endif


    holdoverDuration = buf2uint32(todBuffer+5);
    criticalAlarms = buf2uint16(todBuffer+9);

    minorAlarms = buf2uint16(todBuffer+11);
    //
    // the ICM SMT 360 does not seem to allow for additive leap seconds
    // so we always assume that the leap second bit means a true leap second where
    // time goes backwards.
    //
    leapSecondPending = (minorAlarms & STI_WARN_LEAP_PENDING) ? 0xffU : 0U;

    ppsValid = todBuffer[15] == 0;

    updateStatus();

    return 1U;
}

void IcmSmt360::updateStatus()
{
    uint8_t almostOk = (gpsStatus == 0U) && ((minorAlarms & 0x18U) == 0U) && (disciplineMode == 0U) && (disciplineActivity == 0U);
    allConditionsNormal = haveUtcOffset && almostOk;

    if (disciplineMode == 2 || disciplineMode == 3)
    {
        if (holdoverDuration > maxHoldoverDuration) haveUtcOffset = 0U;
    }    
    
    if (almostOk && !haveUtcOffset) Serial.print(".");
    
    locked = (disciplineMode == 0U) && (disciplineActivity == 0U);

    holdover = 
        (disciplineMode >= 2U)  && (disciplineMode <= 4U) && 
        ((disciplineActivity == 0U) || (disciplineActivity == 8U));
    
    if (holdover)
    {
        holdover = holdoverDuration < maxHoldoverDuration;
    }

}
//
// returns one of the REPLY_xxx consts (see IcmSmt360.h)
//
uint8_t IcmSmt360::parseReply(int8_t length)
{
    if (todBufPtr < 1) return 0U;
    uint8_t rc = REPLY_NONE;

    uint8_t initId = 0xffU;
    uint8_t initSubId = 0xffU;

    if (initIndex < sizeof(initLengths))
    {
        initId = initMsgs[initIndex][1];
        initSubId = initMsgs[initIndex][2];
    }

    switch (todBuffer[0])
    {
    case 0x13:
        break;  // invalid packet; return failure status code
    case 0x8f:
        //Serial.println("8F");
        if (length < 2) break;
        switch(todBuffer[1])
        {
        case 0xa2:
            // confirmation of UTC time units request
            if (length != 3) break;
            if (initId == 0x8e && initSubId == 0xa2 && todBuffer[2] == 0x03) rc = REPLY_INIT;
            break;
        case 0xa5:
            // confirmation of broadcast mask
            if (length != 6) break;
            if (initId == 0x8e && initSubId == 0xa5 && (todBuffer[2] == 0U) && (todBuffer[3] == 0U)) 
                rc = REPLY_INIT;
            break;
        case 0xa6:
            // confirmation of self survey request
            // reply consists of a zero (original command byte) and a zero (command accepted)
            if (length != 4) break;
            if (initId == 0x8e && initSubId == 0xa6 && (todBuffer[2] == 0U) && (todBuffer[3] == 0U))
                rc = REPLY_INIT;
            break;
        case 0xa8:
            if (length != 11) break;
            if (todBuffer[2] != 2) break;
            if (initId == 0x8e && initSubId == 0xa8) 
            {
                // verify that the proper parameter values are returned in the ack
                if (memcmp(todBuffer+3, msgDiscipline+4, 8U) == 0)
                {
                    rc = REPLY_INIT;
                }
            }
            break;
        case 0xab:
            //Serial.println("Got PTI");
            if (parsePTI(length)) rc = REPLY_PTI;
            break;
        case 0xac:
            //Serial.println("Got STI");
            if (parseSTI(length)) rc = REPLY_STI;
            break;
        default:
            break;
        }
        break;
    default:
        break;  // unknown/unrequested packed;
    }

    return rc;
}

void IcmSmt360::printReply(uint8_t length)
{
    Serial.print("! Rcv:");
    for (uint8_t k=0; k<length; k++) 
    {
        Serial.print(" "); 
        Serial.print(todBuffer[k], HEX);
    }
    Serial.println();
}

uint8_t IcmSmt360::Step()
{
    uint8_t rc = REPLY_NONE;

    int8_t n = (int8_t)getReply(); // try to get a serial message

    uint8_t badData = n < 0;
    uint8_t timedOut = TIMEDOUT(millis(), queryTimeout);
    uint8_t next = TIMEDOUT(millis(), nextQueryTime);

    if (n > 0)
    {
        // 
        // a full message was received
        //
        // printReply(n);
        rc = parseReply(n);
        ResetInputBuffer();
        //
        // at start-up, the query interval is shorter to detect GPS lock-up
        // and acquisition of valid TOD with less latency. Once that occurs, we can
        // increase the query interval to its normal value.
        //
        if (allConditionsNormal)
        {
            if (goodQueries < 0xffU) goodQueries++;
            if (goodQueries >= 6) 
            {
                queryInterval = defaultQueryInterval;
            }
        }
    }

    switch(requestState)
    {
    case STATE_SEND_INIT:
        if (!next) break;
        ioInProcess = 1U;
        write(initMsgs[initIndex], initLengths[initIndex]);
        queryTimeout = millis() + QUERY_TIMEOUT;
        requestState = STATE_RCV_INIT;
        // Serial.print("! Send init "); Serial.println(initIndex);
        break;
    case STATE_RCV_INIT:
        if (rc == REPLY_INIT)
        {
            ioInProcess = 0U;
            Serial.print("! Init "); Serial.println(initNames[initIndex]);
            initIndex++;
            if (!clientWantsSurvey && (initIndex == SURVEY_MSG_INDEX))
            {
                initIndex++;
            }
            //
            // the init message was ack'ed; initIndex is currently indexes the current message being sent
            //
            if (initIndex >= sizeof(initLengths))
            {
                //
                // init is complete
                //
                initIndex = 0xff;
                nextQueryTime = millis() + 2000UL;
                requestState = STATE_RQST_PTI;
                Serial.println("! Init Complete");
            }
            else
            {
                //
                // schedule the next init message
                //
                requestState = STATE_SEND_INIT;
                nextQueryTime = millis() + 100UL;
            }
        }
        if (timedOut)
        {
            //
            // retry the current init message
            //
            requestState = STATE_SEND_INIT;
            nextQueryTime = millis() + 100UL;
        }
        break;
    case STATE_RQST_PTI:    
        if (!next) break;
        ioInProcess = 1U;
        write(msgQueryPTI, sizeof(msgQueryPTI));
        queryTimeout = millis() + QUERY_TIMEOUT;
        //Serial.println("2RPTI");
        requestState = STATE_RCV_PTI;
        break;
    case STATE_RCV_PTI:
        if (rc == REPLY_PTI)
        {
            //Serial.println("2QSTI");
            requestState = STATE_RQST_STI;
            nextQueryTime = millis() + queryInterval;
            ioInProcess = 0U;
            rc = 1U;
            break;
        }
        if (timedOut) 
        {
            //Serial.println("2RPTI");
            requestState = STATE_RQST_STI;
            nextQueryTime = millis() + 3000UL;
            ioInProcess = 0U;
        }
        break;
    case STATE_RQST_STI:
        if (!next) break;
        ioInProcess = 1U;
        write(msgQuerySTI, sizeof(msgQuerySTI));
        queryTimeout = millis() + QUERY_TIMEOUT;
        //Serial.println("2RSTI");
        requestState = STATE_RCV_STI;
        break;
    case STATE_RCV_STI:
        if (rc == REPLY_STI)
        {
            //Serial.println("Got STI");
            requestState = STATE_RQST_PTI;
            nextQueryTime = millis() + queryInterval;
            ioInProcess = 0U;
            rc = 1U;
            break;
        }
        if (timedOut)
        {
            //Serial.println("2QSTI");
            requestState = STATE_RQST_PTI;
            nextQueryTime = millis() + 3000UL;
            ioInProcess = 0U;
        }
        break;
    default:
        //Serial.println("SHIT");
        requestState = STATE_RQST_PTI;
        nextQueryTime = millis() + 3000UL;
        ioInProcess = 0U;
        break;
    }

    return rc;
}

#endif
