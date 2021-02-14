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
#if SYSCLK_DISCIPLINED == 0

#include "FreqIirFilter.h"

FreqIirFilter::FreqIirFilter()
{
    Count = 0U;
}

void FreqIirFilter::Preset(int32_t inputValue)
{
    //
    // in general, presets are as follows for a 2nd order section implemented in
    // direct form II transposed structure. z2 is value of delay block fed by 
    // gains b2,a2 and z1 is value delay block fed by a1,b1,z2. 
    // x is the constant input for which the filter is being setup.
    //
    // g = (b2+b1+b0)/(a2+a1+a0)
    // z2 = x (b2 - g a2)
    // z1 = x (b1 - g a1 + z2)
    //
    // these values are custom-computed for the current filter and don't quite seem
    // to work correctly because there is a big disturbance when the filter starts...?
    //
    IirTaps[0] = -5886L * inputValue;
    IirTaps[1] =  6186L * inputValue;
    IirTaps[2] =  7777L * inputValue;
    Count = 1;
}

void FreqIirFilter::Clear()
{
    IirTaps[0] = 0UL;
    IirTaps[1] = 0UL;
    IirTaps[2] = 0UL;
    Count = 0U;
}

//
// This is a direct from II transposed implementation 
// of a 3rd order IIR filter using one 2nd order section
// followed by a 1st order section.
//
int32_t FreqIirFilter::Step(int32_t inputValue)
{
    if (inputValue < -20000L) 
        inputValue = -20000L;
    else
        if (inputValue > 20000L) 
            inputValue = 20000L;
        else 
            ;

    if (Count == 0)
    {
        Preset(inputValue);
    }
    if (Count < MaxCount) Count++;

    // scale input and output values for computations

    int32_t x20 = inputValue * 114; //78;
    int32_t x1  = inputValue * -213; //-155;
    //
    // compute output value
    //
    int32_t y = x20 + IirTaps[0];
    //
    // scale output value
    //
    int32_t y1 = (y << 1) - ((y+10L) / 20L);
    int32_t y2 = -y + ((y+10L) / 21L);
    //
    // compute and store input values to IIR tap registers
    //
    IirTaps[0] = IirTaps[1] + x1 + y1;
    IirTaps[1] = x20 + y2;
    // 
    // that finishes first stage...output is in y
    // scale for input to stage 2
    //
    int32_t yin = (y + 16L) >> 5;
    //
    // compute 2nd stage output
    //
    int32_t z = yin + IirTaps[2];
    //
    // scale output value for feedback
    //
    int32_t z1 = z - (4*z +20L) / 81L;
    //
    // update delay tap for this stage
    //
    IirTaps[2] = yin + z1;
    //
    // re-scale output value -- minor tweak so DC gain is very close to 64.000
    //
    z += (z * 7L + 128L) / 256L;

    return (z + 64L) >> 7; //  (z + 2048L) >> 12;
}

uint16_t FreqIirFilter::FillCount() { return Count; }

uint16_t FreqIirFilter::Filled() { return Count >= MaxCount; }

#endif
