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
// ========================================================================
// 
// Credits:
// 
// The author of this software wishes to thank Andrew Rodland of Madison, WI
// for ideas presented on the following internet web page:
//     http://cleverdomain.org/yapc2012/
// and in software made available at the following URL:
//     http://git.cleverdomain.org/arduino/
// 
// =========================================================================
// 
// 

#pragma once

#include <Arduino.h>
#include <inttypes.h>


class FractionalN
{
public:
    FractionalN(uint8_t NumFractionalBits, uint16_t NominalWholeDivisor);

    ~FractionalN();

    void SetDivisor(int32_t N);
    void SlewToCycle(uint16_t Cycle);
    uint32_t GetTotalTicks();

    virtual void TimerMatchIsr();
    
    uint8_t InterruptCount();

protected:
    
    typedef struct _FracNState
    {
        uint16_t c;         // current timer count
        uint16_t m;         // whole part of divisor
        uint16_t k;         // fractional part of divisor (it is really k/n but n is fixed and not stored here)
        uint16_t s;         // running sum used to track extra counts and accurate phase
        uint8_t  bump;      // true if an extra count will be added in the current timer cycle
        uint8_t  flags;     // only used for word alignment now but could have data in the future.
        uint16_t subCycle;  // simple modulo-n count of timer cycles
        uint32_t cycle;     // number of whole frac-N cycles expired since time zero
        //
        // in C++ we must have these constructors defined, even though we treat the struct like a "C" struct
        // and never assign one such struct to another such struct...
        //
        _FracNState(const _FracNState&) { }
        _FracNState(const volatile _FracNState&) { }
        _FracNState() { }
    } FracNState, *pFracNState;

    // 
    // computes accurate phase for any timer count within the frac-N cycle
    // the result is phase relative to the start of the current timer cycle
    // phase is provided in user-specified units defined by Tc and Td.
    // Td is the amount of phase in one full NOMINAL timer cycle
    // Tc is the amount of phase in one ACTUAL timer tick -- but for many practical cases, a NOMINAL tick can be used instead.
    //
    // To get phase relative to other points the user must apply other calculations
    // based on the current frac-N state. An example would be getting phase relative to the
    // nearest one-second boundary; see NtpFractionalN class for that implementation.
    //
    uint32_t ComputePhase(pFracNState state, uint32_t Tc, uint32_t Td);

    volatile FracNState state;
    //
    // we also track total number of ticks, incremented by actual timer count every 
    // timer cycle. this value is not affected by any frac-N settings.
    //
    volatile uint32_t totalTicks;

    //uint16_t wholeDivisor;
    //uint16_t fractionalDivisor;
    uint16_t nominalDivisor;
    //uint16_t bumpCount;
    
    volatile uint8_t interruptCount;

    //volatile uint32_t wholeCycles;
    //volatile uint16_t cycleAccum;
    //volatile uint16_t fractionalAccum;

    uint8_t  wholeShift;
    uint16_t fractionalMod;
    int32_t  fractionalMask;
    uint16_t rounding;
};
