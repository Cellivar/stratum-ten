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

#include "NtpConfig.h"
#include "SymmetricFirFilter.h"
#include "FreqIirFilter.h"

#include "NtpFractionalN.h"

class NtpPhaseLockedLoop
{

public:
    NtpPhaseLockedLoop(NtpFractionalN *FracN);
    ~NtpPhaseLockedLoop();

    void Init(); // resets initial timers and such...

    void PhaseCaptureIsr();

    uint8_t AllConditionsNormal();

    uint8_t Locked();

    uint8_t Step(); // returns true if interrupt was pending and processed.

    uint8_t Holdover();

private:

    void PpsLost();

    uint8_t holdoverCount = 0U;

    //
    // these variables are used to measure frequency by counting total timer ticks in a 32-bit
    // unsigned variable. This value is unaffected by tick adjustments and is a raw count of
    // the uncorrected timer value. At a 2MHz clock rate, this will overflow about every 36 minutes
    //
    uint8_t  prev_pps_valid = 0U;
    uint32_t prev_pps_value = 0UL;
    uint32_t pps_timeout = 0UL;         // millis()-based timeout to detect loss of PPS signal
    uint32_t holdover_timeout = 0UL;    // millis()-based also
    uint8_t  holdover_timed_out = 0U;
    //
    // timer state variables and flags
    //
    volatile uint16_t capture_time = 0; // timer value recorded by input capture hardware from PPS signal
    volatile uint8_t capture_flag;         // set true by ISR to let background loop know that a PPS has occurred
    volatile uint8_t phase_initialized = 0;

    //
    // this is really integrated phase error in units of ns. is thie the "I" part of the "PI" control loop?
    // since one timer tick is 500ns and the frequency is 1Hz, this is a very small phase error unit...???
    //
    int32_t integrator = 0;          // integrated phase error in units of ns

    uint8_t settled = 0;
    uint8_t iir_settled = 0;

    //
    // half of a 16-tap symmetric FIR filter impulse response
    // coeffs are multiplied by 32768, so shift the summed result right 15 bits.
    //
    int16_t pll_filter[8] = {
        -158, 953, 1236, 1810, 2433, 3002, 3436, 3672
    };

    SymmetricFirFilter pllClockFilter;

    //
    // DLL filters have a sum of 16,348 but the default averaging has a sum of only 64,
    // so the results need to be divided by 16384/64 = 256.
    // For new filters with different sums, change the divisor accordingly.
    //
    // We've also tried very narrow IIR filters here but too low of a cutoff 
    // frequency seems to actually make matters worse. The small amount of dithering seen
    // in the output of this filter in normal operation actually seems to improve the
    // result slightly (std dev calcs on phase offsets drops maybe 10ns from 162 to 152).
    //
    int16_t freq_fir_filter[32] = {
        203,67,77,88,100,112,125,139,152,167,181,196,211,226,241,256,271,285,299,
        312,325,337,348,358,368,376,384,390,395,399,401,403
    };

    SymmetricFirFilter freqFirFilter;

    FreqIirFilter freqIirFilter;
    uint8_t iirEnabled;

#ifdef HOLDOVER_PREDICTION
    //
    // holdover -- to estimate frequency ramp rate during holdover
    //
    static const uint8_t   freqHistoryLength = 11U;     // freq history FIFO length
    static const uint8_t   freqHistoryInterval = 60U;   // how often is freq history saved for holdover, in seconds
    
    // how many ms wortho of freq history is in the freqHistory FIFO?
    static const int32_t   freqHistorySpanMs = 1000L * (int32_t)freqHistoryInterval * (int32_t)(freqHistoryLength - 1U);
    
    // half of the above for use in rounding the result of divisions.
    static const int32_t   freqHistorySpanRounding = freqHistorySpanMs >> 1;
    
    // how often is the frac-N divisor updated during holdover?
    static const uint32_t   holdoverUpdateIntervalMs = 10000UL;

    int32_t         freqHistory[freqHistoryLength];
    int16_t         freqHistoryTicker = -240;      // don't start collecting for 5 minutes
    uint8_t         freqHistoryCount = 0U;
    uint32_t        lastFreqHistory = 0U;
    uint32_t        holdoverStartTime = 0UL;
    uint32_t        nextHoldoverCorrection = 0UL;
#endif

    NtpFractionalN *fracN;
};
