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

#include "DiscreteTimeFilter.h"

class FreqIirFilter : DiscreteTimeFilter
{

public:
    FreqIirFilter();

    int32_t Step(int32_t InputValue);

    void Preset(int32_t InputValue);

    void Clear();

    uint16_t FillCount();
    uint16_t Filled();

private:
    
    const uint16_t MaxCount = 280;  // settling time
    int32_t IirTaps[3] = {0, 0, 0};
    uint16_t Count;

};



