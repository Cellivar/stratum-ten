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
#if SYSCLK_DISCIPLINED == 0

#include <Arduino.h>
#include "FractionalN.h"
#include "NtpHdwr.h"

FractionalN::FractionalN(uint8_t NumFractionalBits, uint16_t NominalWholeDivisor)
{
    nominalDivisor = NominalWholeDivisor;
    wholeShift = NumFractionalBits;
    fractionalMod = 1L << wholeShift;
    fractionalMask = fractionalMod - 1L;
    rounding = 1L << (wholeShift - 1);
    
    interruptCount = 0U;

    state.c = 0U;
    state.m = nominalDivisor;
    state.k = 0U;
    state.s = 0U;
    state.bump = 0U;
    state.flags = 0U;
    state.subCycle = 0U;
    state.cycle = 0UL;

    totalTicks = 0U;
}

FractionalN::~FractionalN()
{
}

uint8_t FractionalN::InterruptCount()
{
    if (interruptCount == 0) return 0;
    return interruptCount--;
}

uint32_t FractionalN::GetTotalTicks() { return totalTicks; }

//
// N is a fixed point fraction with "NumFractionalBits" fractional bits.
// 
void FractionalN::SetDivisor(int32_t N)
{
    state.k = (uint16_t)(N & fractionalMask);

    int32_t whole = N >> wholeShift;

    if (whole < 0)
    {
        state.m = nominalDivisor - (uint16_t)(-whole);
    }
    else
    {
        state.m = nominalDivisor + (uint16_t)whole;
    }

    SET_FRAC_N_INTERVAL(state.m + state.bump);
    
}

void FractionalN::SlewToCycle(uint16_t Cycle)
{
    state.subCycle = Cycle;
    state.cycle = 0U;
    totalTicks = 0U;
}

void FractionalN::TimerMatchIsr()
{
    state.subCycle++;
    totalTicks += state.m + state.bump;
    //
    // count the number of partial and whole timer cycles modulo-fractionalMod (e.g. mod 2048)
    // this count is unaffected by changes in the divisor...just a raw count of the number
    // of timer cycles.
    //
    if (state.subCycle >= fractionalMod)
    {
        state.subCycle = 0U;
        state.cycle++;
    }
    //
    // don't reduce the accumulator until AFTER an extra count has been clocked.
    // only then has the accumulated time error actually been compensated.
    // this permits easy calculation of accurate times from timer counts.
    // if bumpCount is non-zero then an extra count was added in the previous cycle
    // (the one this interrupt was triggered by).
    //
    if (state.bump) state.s -= fractionalMod;

    state.s += state.k;
    //
    // if the fractional count accumulator >= to the frac-N fractional divisor (e.g. 2048)
    // then add a count in the upcoming cycle.
    //
    if (state.s >= fractionalMod) 
    {
        state.bump = 1U;
    }
    else
    {
        state.bump = 0U;
    }

    SET_FRAC_N_INTERVAL(state.m + state.bump);

    interruptCount++;
}

uint32_t FractionalN::ComputePhase(pFracNState state, uint32_t Tc, uint32_t Td)
{
    // 
    // first we compute time assuming that division by M is perfect (e.g. frac-N not required)
    // then we compute correction for current state of frac-N divider.
    //
    // ====== Some careful integer math to accurately compute time from timer count ============
    //
    // This is the coarse value not corrected for frac-N state.
    // What we want to compute is how many ns is represented by the current timer count
    // value "C", assuming that a count of "M" ticks represents "Td" ns of time. 
    // This does not include corrections for frac-N accumulated fraction.
    //
    // With floating point math this would be easy, ns = Td x C / M. 
    //
    // The method described below computes the result accurate to +-1/2ns using only integer math.
    //
    // Let Td = nominal time per interrupt (31,250,000 for example)
    //     M  = number of timer ticks per interrupt (OCR4 + 1, for example 62,500)
    //     T  = true time per timer tick in ns (not an integer value in most cases)
    // We wish to calculate the number of ns expired in "C" timer ticks based
    // on the above values. Because T contains a fractional part and "n" can range
    // as high as 62,000 or more, if we ignore the fractional part of T the result 
    // may be in error as much as 30,000ns (30us) or more.
    //
    // For an extreme example, let Td = 31,250,000, M = 62,563 and T = Td/M = 499.4965ns
    //
    // To deal with this, we compute a fractional part too as follows.
    //
    // Let D = floor(T) = floor(Td/M)
    // The fractional remainder from this integer division, R = Td/M - D.
    // Multiply both sides of the equation by M and we have
    // R x M = Td - D x M and this value (R x M) is exact.
    // So now we can say that the integer and fractional parts of T are:
    //
    // >>>>> T = D + (Td - D x M) / M  <<<<< and this is also exact.
    //
    // Next, define Rm = (R x M) = (Td - D x M)
    //
    // Finally, to compute expired ns at some count, "C" using integer math
    // we can do as follows:
    //
    // ns = C x D + (C x Rm + M/2) / M
    //
    // the result will be accurate to +-1/2ns (because M/2 introduces rounding instead of truncation)
    // this will all work using uint32_t math because the value M always fits into a 16-bit unsigned int
    // (and is typically close to 62,500).
    // 
    uint32_t D = Td / state->m;
    uint32_t Rm = Td - D * state->m;
    uint32_t time = state->c * D + (state->c * Rm + (state->m >> 1)) / state->m;
    //
    // now adjust for the current frac-N state
    //
    uint32_t sum = state->s + (uint32_t)state->k * state->c / state->m;
    uint32_t delta = ((uint32_t)Tc * sum + rounding) >> wholeShift;

    return time - delta;
}

#endif
