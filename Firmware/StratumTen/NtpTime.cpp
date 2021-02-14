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

#include "NtpTime.h"


uint32_t NtpTime::JulianDay(uint16_t year, uint8_t month, uint8_t day)
{
    if (month < 3)
    {
        month += 12;
        year--;
    }

    int a = year / 100;
    int b = 2 - a + (a >> 2);
    //
    // 365.25 * 4 = 1461
    // t1 is equivalent to floor(365.25 * (year + 4714))
    //
    int32_t t1 = (1461L * (year + 4716)) >> 2;
    int16_t t2 = (306 * (month + 1)) / 10;
    uint32_t jd = t1 + t2 + day + b - 1524L;

    return jd;
}

uint32_t NtpTime::CalendarToNtpSeconds(
    uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec)
{
    InitEpochs();
    //
    // compute seconds between NTP epoch and the beginning of the current day
    //
    uint32_t jd = JulianDay(year, month, day);
    uint32_t seconds = (jd - jd_of_ntp_epoch) * 86400UL;
    //
    // add seconds transpired in the current day
    //
    seconds += (hour * 60UL + min) * 60UL + sec;

    return seconds;
}

uint32_t NtpTime::GpsToNtpSeconds(uint16_t week, uint32_t seconds)
{
    InitEpochs();
    //
    // this one is easy; just add the difference between GPS and NTP epochs 
    // in seconds (there are 604,800 seconds in a week)
    //
    return gps_epoch_in_ntp_seconds + (uint32_t)week * 604800UL + seconds;
}

void NtpTime::InitEpochs()
{
    //
    // compute julian days corresponding to relevant epochs only once and save them
    //
    if (jd_of_ntp_epoch == 0L)
    {
        jd_of_gps_epoch = JulianDay(1980, 1, 6); 
        jd_of_ntp_epoch = JulianDay(1900, 1, 1);
        gps_epoch_in_ntp_seconds = (jd_of_gps_epoch - jd_of_ntp_epoch) * 86400UL;
    }
}

