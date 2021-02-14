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

//
// Implements a FIR filter whose impulse response is symmetric 
// about the center. That is, if coeffs are numbered 0..(N-1) then
// symmetric means that h(0) == h(N-1) and in general h(k) == h(N-1-k).
// Special treatment is applied for odd filter lengths which have a
// central impulse response coefficient which is not repeated.
//
class DiscreteTimeFilter
{

public:
    virtual int32_t Step(int32_t InputValue) = 0;

    virtual void Preset(int32_t InputValue) = 0;

    virtual void Clear() = 0;

    virtual uint16_t FillCount() = 0;
    virtual uint16_t Filled() = 0;

private:

};
