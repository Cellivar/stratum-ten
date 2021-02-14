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

#if GPS_IS_ICM_SMT_360

#include <Arduino.h>
#include <inttypes.h>
#include <HardwareSerial.h>

#include "SerialGps.h"
#include "NtpTime.h"
#include "TrimbleTSIP.h"

class IcmSmt360 : public SerialGps
{

public:

    IcmSmt360(HardwareSerial* ComPort);
    
    void Init();
    void Init(uint8_t RequestSurvey);
    void FlushInput();
    uint8_t IoInProcess();
    //
    // return values for Step() and also internally for parseReply()
    // although this is public, it is NOT part of the SerialGps parent class
    // and in many cases will not be used by the client.
    //
    enum { REPLY_NONE = 0, REPLY_INIT = 1, REPLY_PTI = 2, REPLY_STI = 3 };

    uint8_t Step();
    //
    // locked requires 
    // 1. discipline mode is normal or holdover
    // 2. discipline activity is phase locking
    // 3. UTC offset is valid. From power-up, it can take as long as 12 minutes to
    //    acquire the current UTC offset from the ICM SMT 360 -- it does not save the
    //    current UTC offset in flash/EEPROM and must be acquired fresh with each power cycle.
    // locked does not require any survey in progress be finished because:
    // 1. If module detects antenna move > 100m it should start new survey and hopefully not inidicate
    //    that PPS is good.
    // 2. If unit is not moved it will use last survey posn for timing until survey is complete.
    //
    uint8_t Locked();
    uint8_t Holdover();

protected:

    void ResetInputBuffer();
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
    uint8_t queryTimeOfDay();
    uint8_t queryStatus();
    uint8_t queryDetailedStatus();

    void write(uint8_t *msg, uint8_t count);
    int getReply();
    void printReply(uint8_t length);
    //
    // methods to convert big-endian buffer data into little-endian usable quantities
    //
    uint32_t buf2uint32(uint8_t *buf);
    uint16_t buf2uint16(uint8_t *buf);
    int16_t buf2int16(uint8_t *buf);
    float buf2float(uint8_t *buf);
    float bufdouble2float(uint8_t *buf);

    uint8_t parsePTI(int8_t length);
    uint8_t parseSTI(int8_t length);

    uint8_t parseReply(int8_t length);

    void updateStatus();

    uint8_t todBuffer[80];
    int8_t todBufPtr = -1;
    uint8_t som = 0;            // count of DLE chars received during current message; used to detect end packet
    
    uint8_t initIndex = 0U;
    uint8_t requestState = 1U;  // cycles between the various possible query operations (tod/status/detailed status)    
    uint8_t ioInProcess = 0U;
    //
    // data taken from the PTI/STI packets
    //
    uint8_t haveUtcOffset = 0U;
    uint16_t criticalAlarms = 0xffffU;
    uint16_t minorAlarms = 0xffffU;
    
    uint8_t surveyProgress = 0U;
    uint8_t clientWantsSurvey = 0U;
    uint8_t surveyPosValid = 0U;
    float surveyLatitude = 0.0;
    float surveyLongitude = 0.0;
    float surveyAltitude = 0.0;
    const float DEG_PER_RAD = 57.295779513;

    uint32_t holdoverDuration = 0xffffffffUL;
    uint8_t gpsStatus = 8U;
    uint8_t disciplineMode = 1U;
    uint8_t disciplineActivity = 6U;

    uint32_t queryTimeout = 0UL;
    uint32_t nextQueryTime = 0UL;
    uint32_t queryInterval = 5000UL;
    uint8_t  goodQueries = 0U;

    SuppTiming suppTiming;

    const uint32_t maxHoldoverDuration  = 3600UL;   // in seconds, 3600 is one hour
    const uint32_t defaultQueryInterval = 131097UL;  

    //
    // ================= Trimble TSIP protocol messages ===========================
    //
    const uint8_t SOM = 0x10;   /* DLE char is start packet byte */
                                /* if actual message data contains PSTART, then PSTART is sent twice to signify that */
    const uint8_t EOM = 0x03;   /* ETX char is end packet byte, but only when preceded immediately by an PSTART byte */

#define EMS     SOM, EOM    /* end-message sequence is two bytes: 0x10, 0x03 */
    
    uint8_t msgBroadcastOff[9] = { SOM, 0x8e, 0xa5, 0x00, 0x00, 0x00, 0x00, EMS };
    uint8_t msgUtcTiming[6] = { SOM, 0x8e, 0xa2, 0x03, EMS };
    //
    // bytes 4..7 are the jam sync threshold in ns, floating point; setting to 0.0 disables jam sync
    // bytes 8..11 are the max freq offset in ppb, floating point; set to 10, which seems to be maximum
    //              10ppb offset allows recovery of 10nsec of drift error per second, or 36us/hour
    //
    uint8_t msgDiscipline[14] = { 
        SOM, 0x8e, 0xa8, 0x02, 
        0x00, 0x00, 0x00, 0x00,         // 0.0 in 32-bit IEEE float, big-endian
        0x41, 0x20, 0x00, 0x00, EMS     // 10.0 in 32-bit IEEE float, big-endian
    };

    uint8_t msgSelfSurvey[6] = { SOM, 0x8e, 0xa6, 0x00, EMS };    // requests new position survey

    uint8_t msgQueryPTI[6] = { SOM, 0x8e, 0xab, 0x01, EMS };     // requests PTI packet sent on next second
    uint8_t msgQuerySTI[6] = { SOM, 0x8e, 0xac, 0x00, EMS };     // supplemental timint information request, to be sent immediately (on next second does not work?)
    //
    // this array of message pointers determines which init messages are sent at power-up and reset
    // The ::parseReply() method MUST include code to detect and flag the arrival of acks for these
    // messages because that's how the ::Step() state machine knows to move on to the next init message,
    // or to re-send the current init message.
    //
    uint8_t *initMsgs[4] = { msgBroadcastOff, msgSelfSurvey, msgUtcTiming, msgDiscipline };

    const uint8_t SURVEY_MSG_INDEX = 1U;

    uint8_t initLengths[4] = { 
        sizeof(msgBroadcastOff), sizeof(msgSelfSurvey), sizeof(msgUtcTiming), sizeof(msgDiscipline) 
    };

    char *initNames[4] = { "broadcast", "start survey", "UTC units", "discipline" };

    char *disciplineModeNames[7] = { "Locked", "Power-Up", "Auto Holdover", "Manual Holdover", "Recovery", 0, "Disabled" };
    char *disciplineActivityNames[10] = { "Phase Locking", "Warm-Up", "Frequency Locking", "Placing PPS", "Loop Filter Init", 
                                      "Holdover", "Inactive", 0, "Recovery", "Calibration" };
    char *gpsStatusNames[17] = { "Locked", "No time", 0, "PDOP too high", 
                             0, 0, 0, 0,
                             "No sats", "Only one sat", "Only two sats", "Only 3 sats",
                             "Sat unusable", 0, 0, 0, 
                             "TRAIM fail" };
};

#endif