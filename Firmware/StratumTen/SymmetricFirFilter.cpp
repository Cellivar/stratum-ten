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

#include <Arduino.h>
#include "SymmetricFirFilter.h"

SymmetricFirFilter::SymmetricFirFilter(int16_t *ImpulseResponse, uint8_t Length, int8_t Log2OutputScaling)
{
    h = ImpulseResponse;
    N = Length;
    Nsym = N >> 1;
    odd = N & 0x01;
    leftShifts = rightShifts = 0;
    rounding = 0L;

    if (Log2OutputScaling > 0)
    {
        leftShifts = Log2OutputScaling;
    }
    else
    {
        rightShifts = -Log2OutputScaling;
        rounding = 1L << (rightShifts - 1);
    }

    fill = 0U;
    moveCount = (N - 1) * sizeof(int32_t);
    x = new int32_t[N];
}

SymmetricFirFilter::~SymmetricFirFilter()
{
    delete[] x;
}

int32_t SymmetricFirFilter::Step(int32_t InputValue)
{
    // 
    // shift history right one step, then copy new value into x[0]
    //
    memmove(x+1, x, moveCount);
    x[0] = InputValue;
    //
    // compute FIR sum
    //
    int32_t sum = 0L;
    uint8_t mirror = N - 1;
    for (uint8_t k=0; k<Nsym; k++)
    {
        sum += h[k] * ((int32_t)x[k] + (int32_t)x[mirror--]);
    }
    
    if (odd) sum += (int32_t)h[Nsym] * x[Nsym];

    if (fill < N) fill++;

    if (leftShifts > 0)
        return sum << leftShifts;
    else
        return (sum + rounding) >> rightShifts;
}

void SymmetricFirFilter::Preset(int32_t InputValue)
{
    for (uint8_t k=0; k<N; x[k++] = InputValue);
    fill = N;
}

void SymmetricFirFilter::Clear()
{
    fill = 0;
}

uint16_t SymmetricFirFilter::FillCount() 
{ 
    return fill; 
}

uint16_t SymmetricFirFilter::Filled() 
{ 
    return fill >= N;
}

#endif
