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
#include "DiscreteTimeFilter.h"

//
// Implements a FIR filter whose impulse response is symmetric 
// about the center. That is, if coeffs are numbered 0..(N-1) then
// symmetric means that h(0) == h(N-1) and in general h(k) == h(N-1-k).
// Special treatment is applied for odd filter lengths which have a
// central impulse response coefficient which is not repeated.
//
class SymmetricFirFilter : DiscreteTimeFilter
{

public:
    //
    // Length is the total length of the impulse response (e.g. 65). 
    // The ImpulseResponse array must be of length (Length+1) >> 1 (e.g. 33)
    // Results are shifted left or right as controlled by Log2OutputScaling
    // Right shifts include rounding instead of truncation.
    //
    SymmetricFirFilter(int16_t *ImpulseResponse, uint8_t Length, int8_t Log2OutputScaling);
    ~SymmetricFirFilter();

    int32_t Step(int32_t InputValue);

    void Preset(int32_t InputValue);

    void Clear();

    uint16_t FillCount();
    uint16_t Filled();

private:
    int16_t *h;
    uint8_t N;
    uint8_t Nsym;           // number of symmetric entries, floor(N/2)
    uint8_t odd;            // true if length is odd
    //
    // output scaling information
    //
    uint8_t leftShifts;
    uint8_t rightShifts;
    int32_t rounding;

    size_t moveCount;   // number of bytes to shift for each filter step operation
    uint16_t fill;       // how many entries have been received? (zero to N but no more)
    int32_t *x;         // input sequence history buffer
};
