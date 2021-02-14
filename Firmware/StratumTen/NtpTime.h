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

class NtpTime
{

public:
    //
    // ===>>> conversion from UTC date/time to NTP time <<<===
    //
    // We deal with this by first computing the julian day, then subtracting
    // the julian day of the NTP epoch. This gives days since NTP epoch and 
    // multiplying by secs/day and adding seconds expired in current day gives time in NTP seconds.
    //
    // The NTP epoch is Jan 1, 1900.
    // The GPS epoch is Jan 6, 1980.
    //

    //
    // Returns the exact julian day for noon on the indicated date.
    // For JD at midnight on the date, subtract 0.5 from the integer result.
    // Algorithm is from Meeus, modified to work with integer math.
    //
    uint32_t JulianDay(uint16_t year, uint8_t month, uint8_t day);

    uint32_t CalendarToNtpSeconds(
        uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec);

    uint32_t GpsToNtpSeconds(uint16_t week, uint32_t seconds);

private:
    void InitEpochs();
    //
    // julian day of importan epochs
    //
    uint32_t jd_of_gps_epoch = 0UL;  // not used now but would be of use if date/time is in GPS weeks
    uint32_t jd_of_ntp_epoch = 0UL;
    uint32_t gps_epoch_in_ntp_seconds = 0UL;

};
