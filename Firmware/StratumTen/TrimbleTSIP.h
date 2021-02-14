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

#include <inttypes.h>

//
// by default:
//
// broadcast of PTI/STI is enabled.
// overdetermined clock mode is on
// max sens, anti-jamming enabled
// elevation mask zero degrees (consider increasing)
// signal level mask 0.0/No (no minimum level)
// PPS is enabled rising edge and not gated by satellite status
// All timing is aligned to GPS time, not UTC (use 
//
#pragma pack 1

//
// ===== TIME UNITS =====
//
typedef struct
{
    byte Id = 0x8E;
    byte SubId = 0xA2;
    byte Flag;
} TimeUnits, *pTimeUnits;
//
// Flag values
//
#define TUFLAG_TIME_UTC         0x01
#define TUFLAG_PPS_UTC          0x02
#define TUFLAG_TIME_SET         0x04
#define TUFLAG_UTC_VALID        0x08
#define TUFLAG_TIME_DETAIL      0x30
#define TUFLAG_PPS_DETAIL       0xC0
//
// values for the details fields (must be shifted into the field)
//
#define TIME_DETAIL_UTC         0
#define TIME_DETAIL_GLONASS     1
#define TIME_DETAIL_BEIDOU      2
#define TIME_DETAIL_GALILEO     3
//
// ===== PRIMARY TIMING =====
//
typedef struct
{
    byte Id = 0x8E;
    byte SubId = 0xAB;
    byte Rqst;   // 0=immediate, 1=next second, 2=next second plus secondary packet
} PrimaryTimingRqst, *pPrimaryTimingRqst;

typedef struct 
{
    byte Id = 0x8F;
    byte SubId = 0xAB;
    byte TimeOfWeek[4];
    byte Week[2];
    byte UtcOffset[2];
    byte Flag;
    byte Seconds;
    byte Minutes;
    byte Hours;
    byte Day;
    byte Month;
    uint16_t Year;
} PrimaryTimingInfo, *pPrimaryTimingInfo;

//
// values for the Flag byte above
//
#define PTI_FLAG_TIME_UTC       0x01;
#define PTI_FLAG_PPS_UTC        0x02;
#define PTI_FLAG_TIME_SET       0x04;
#define PTI_FLAG_UTC_VALID      0x08;
#define PTI_FLAG_TIME_DETAIL    0x30;
#define PTI_FLAG_PPS_DETAIL     0xC0;
//
// ===== SUPPLEMENTAL TIMING =====
//
typedef struct
{
    byte Id = 0x8F;
    byte SubId = 0xAC;
    byte RxMode;                // should be 7 for over-determined clock mode
    byte DisciplineMode;
    byte SurveyPct;
    byte HoldoverSeconds[4];
    byte CriticalAlarms[2];
    byte MinorAlarms[2];
    byte GpsStatus;
    byte DisciplineState;
    byte PpsOkay;               // PPS Okay == 0x01, bad = 0x00
    byte SpareStatus2;
    byte PpsOffsetNs[4];        // float
    byte ClockOffsetPpb[4];     // float
    byte DacValue[4];           // uint32_t
    byte DacVoltage[4];         // float
    byte TemperatureDegC[4];    // float
    byte LatRad[8];             // double latitude radians
    byte LongRad[8];            // double longitude radians
    byte AltitudeMeters[8];     // double
    byte PpsQuantErrNs[4];      // quantization error, ns
    byte Spare[4];
} SuppTiming, *pSuppTiming;

//
// DisciplineMode values
//
#define STI_DM_LOCKED       0
#define STI_DM_POWER_UP     1
#define STI_DM_AUTO_HOLD    2
#define STI_DM_MAN_HOLD     3
#define STI_DM_RECOVERY     4
#define STI_DM_INVALID      5
#define STI_DM_DISABLED     6

class DisciplineMode
{
public: enum { Locked, PowerUp, AutoHold, ManualHold, Recovery, Disabled = 6 };
};
//
// DisciplineState values
//
#define STI_DS_PHASE_LOCKING    0
#define STI_DS_WARMUP           1
#define STI_DS_FREQ_LOCKING     2
#define STI_DS_PPS_SET          3
#define STI_DS_FILTER_INIT      4
#define STI_DS_COMP_OCXO        5
#define STI_DS_INACTIVE         6
#define STI_DS_INVALID          7
#define STI_DS_RECOVERY         8
#define STI_DS_CAL              9

class DisciplineState
{
public: enum { PhaseLock, WarmUp, FreqLock, PlacePps, HoldoverComp, Inactive, Recovery = 8, Cal = 9 };
};

//
// Critical Alarm
//
#define STI_ALARM_DAC           0x10
//
// Minor Alarms
//
#define STI_WARN_DAC            0x0001
#define STI_WARN_ANT_OPEN       0x0002
#define STI_WARN_ANT_SHORT      0x0004
#define STI_WARN_NOTRACK        0x0008
#define STI_WARN_NODISCIPLINE   0x0010
#define STI_WARN_SURVEY         0x0020
#define STI_WARN_NOPOS          0x0040
#define STI_WARN_LEAP_PENDING   0x0080
#define STI_WARN_TEST_MODE      0x0100
#define STI_WARN_POSITION       0x0200
#define STI_WARN_ALMANAC        0x0800
#define STI_WARN_PPS            0x1000

class MinorAlarm
{
public: enum { 
            Dac=1, Open=2, Short=4, NoTrack=8, 
            NoDiscipline=0x10, Survey=0x20, NoStoredPos=0x40, LeapPend=0x80, 
            TestMode=0x100, PosQuality=0x200, Almanac = 0x800, NoPps = 0x1000
        };
};

//
// Gps Status
//
#define STI_GPS_DOING_FIXES     0x00
#define STI_GPS_NO_GPS_TIME     0x01
#define STI_GPS_PDOP_BAD        0x03
#define STI_GPS_NO_SAT          0x08
#define STI_GPS_1_SAT           0x09
#define STI_GPS_2_SAT           0x0A
#define STI_GPS_3_SAT           0x0B
#define STI_GPS_SAT_UNUSABLE    0x0C
#define STI_GPS_TRAIM_FAIL      0x10

class GpsStatus
{
public: enum { 
            DoingFixes, NoGpsTime, PdopHigh=0x03, 
            NoSats=0x08, OneSat=0x09, TwoSats=0x0a, ThreeSats=0x0b,
            RaimFail=0x10
        };
};

//
// ===== BROADCAST MASK =====
typedef struct
{    
    byte Id = 0x8E;
    byte SubId = 0xA5;
    byte Mask0[2];
    byte Mask1[2];
} BroadcastMask, *pBroadcastMask;
//
// broadcast mask 0 
//
#define BCST_PTI        0x01
#define BCST_STI        0x04
#define BCST_AUTO       0x40  /* enables automatic packets selected by I/O options command */
//
// ===== I/O OPTIONS =====
//
typedef struct
{
    byte Id = 0x35;
    byte Position;
    byte Velocity;
    byte Timing;
    byte Aux;
} IoOptions, *pIoOptions;
//
// bits within the above fields
//
#define IO_POS_ECEF     0x01
#define IO_POS_LLA      0x02
#define IO_POS_DATUM    0x04
#define IO_POS_DBL_PREC 0x10

#define IO_VEL_ECEF     0x01
#define IO_VEL_ENU      0x02

#define IO_TIM_UTC      0x01

#define IO_AUX_PKT_5A   0x01
#define IO_AUX_DB_HZ    0x08
//
// ===== RECEIVER CONFIGURATION (MODES) =====
typedef struct
{
    byte Id = 0xBB;
    byte SubId = 0x00;
    byte RxMode;
    byte Reserved1[3] = {0xff, 0xff, 0xff};
    byte ElevMaskRad[4];        // float
    byte CNoMask[4];            // signal level mask for OD clk mode
    byte Reserved2[3] = {0xff, 0xff, 0xff};
    byte AntiJamMode;
    byte Reserved3[4] = {0xff, 0xff, 0xff, 0xff};
    byte Constellation;
    byte Reserved4[12] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
} RxConfig, *pRxConfig;

#define RXCONFIG_MODE_AUTO      0
#define RXCONFIG_MODE_SINGLE    1
#define RXCONFIG_MODE_2D        3
#define RXCONFIG_MODE_3D        4
#define RXCONFIG_MODE_OD_CLK    7

#define RXCONFIG_CONST_GPS      0x01
#define RXCONFIG_CONST_GLONASS  0x02
#define RXCONFIG_CONST_BEIDOU   0x08
#define RXCONFIG_CONST_GALILEO  0x10
#define RXCONFIG_CONST_QZSS     0x20

#endif
