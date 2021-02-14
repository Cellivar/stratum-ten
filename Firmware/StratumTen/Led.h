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

#include <inttypes.h>

class Led 
{

public:
    Led(int Pin);

    void ConfigureFlash(uint16_t OnTimeMs, uint16_t OffTimeMs);

    void On();
    void Off();
    void Set(uint8_t On);
    uint8_t Get();
    //
    // maximum flash count is 7ffe. values greater than this are limited to 0x7ffe.
    // 
    void Flash(uint8_t Count);
    //
    // Calling Set(.) or Flash(.) will terminate continuous flashing
    //
    void FlashContinuous();

    uint8_t Busy();

    uint8_t Update(); // returns busy status (boolean)

private:

    int      pin;
    uint16_t onTime;
    uint16_t offTime;
    uint32_t timeout;
    uint16_t flashCount;
    uint8_t  flashContinuous;
    uint8_t  staticOn;          // when true, causes polarity of flashes to be inverted
};

