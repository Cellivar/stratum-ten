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
#include "NtpConfig.h"

#if SYSCLK_DISCIPLINED == 0

#include "NtpDebug.h"
#include "NtpHdwr.h"
#include "NtpPhaseLockedLoop.h"
//
// Turn this on to watch loop dynamics, but it will cause a 2-300us error in NTP transmit timestamps. 
// This should be set to zero for normal operation.
//
#define DEBUG_CSV 0

NtpPhaseLockedLoop::NtpPhaseLockedLoop(NtpFractionalN *FracN) :
        pllClockFilter(pll_filter, 16U, -15), 
        freqFirFilter(freq_fir_filter, 64U, -8),
        freqIirFilter()
{
    fracN = FracN;
    iirEnabled = 0;
}

NtpPhaseLockedLoop::~NtpPhaseLockedLoop()
{
}

void NtpPhaseLockedLoop::Init()
{
    uint32_t now = millis();
    pps_timeout = now + 10000UL;
    holdover_timeout = now + 600000UL;
    holdover_timed_out = 0U;
    holdoverCount = 0U;
}

uint8_t NtpPhaseLockedLoop::Locked()
{
    return settled >= 60;
}

uint8_t NtpPhaseLockedLoop::AllConditionsNormal()
{
    return settled >= 60;
}

void NtpPhaseLockedLoop::PhaseCaptureIsr()
{    
    if (phase_initialized)
    {
        capture_time = PPS_CAPTURE_COUNT;
        capture_flag = 1;
    }
    else
    {
        //
        // the small offset is hand-tuned to make one particular mega board start up
        // with very small phase errors. for general use, remove the small offset.
        //
        uint16_t t0 = (NOMINAL_INTEGER_DIVISOR >> 1); // - 86U;
        //
        // reset timer counts so we'll be close to the PPS timing and won't have to search a long ways for loop lock-up
        //
        SET_TIMERS_COUNT(t0);

        capture_flag = 0;
        phase_initialized = 1;
    }
}

//
// Called to inform us that a PPS pulse has not arrived on schedule
//
void NtpPhaseLockedLoop::PpsLost()
{
    // are we just entering holdover, or are we already there?
    if (holdoverCount == 0)
    {
        // just entering, so set a timeout for this holdover event
        holdover_timeout = millis() + 600000UL; // ten minutes maximum
    }

    prev_pps_valid = 0;
    holdoverCount = 5;

#ifdef HOLDOVER_PREDICTION
    if (holdoverStartTime == 0UL) 
    {
        holdoverStartTime = lastFreqHistory;
        nextHoldoverCorrection = millis() + holdoverUpdateIntervalMs;
    }
#endif
}

//
// query function to find out if PLL is in holdover mode
//
uint8_t NtpPhaseLockedLoop::Holdover()
{
    return holdover_timed_out || (holdoverCount > 0);
}

uint8_t NtpPhaseLockedLoop::Step()
{
    // 
    // these are adjustments to the frac-N divisor, m+k/n where m is nominally 62,500 and n is 2048.
    //
    // they are fixed-point fractional values with the radix point between bits 10 and 11.
    // as such, a value of 2048 corresponds to an adjustment of one in the frac-N divisor,
    // and a value of 1 corresponds to an adjustment of 1/2048 in the frac-N divisor.
    //
    int32_t freq_correction = 0;
    int16_t phase_correction = 0;

    int k;
    uint32_t now = millis();

    if (!capture_flag)
    {
        if ((holdoverCount == 0) && TIMEDOUT(now, pps_timeout))
        {
            PpsLost();
        }
        //
        // if we're in holdover mode, check for holdover timeout
        //
        if (!holdover_timed_out && (holdoverCount > 0) && TIMEDOUT(now, holdover_timeout))
        {
            holdover_timed_out = 1U;
            settled = 0U;               // force a PLL unlock indication
        }

#ifdef HOLDOVER_PREDICTION
        //
        // if no PPS capture exists, and we're in holdover mode, 
        // apply a frequency ramp correction if possible.
        // if that's not possible just continue with no correction.
        //
        if (!holdover_timed_out && holdoverCount > 0 && freqHistoryCount >= freqHistoryLength)
        {
            if (TIMEDOUT(now, nextHoldoverCorrection))
            {
                uint32_t elapsed = now - holdoverStartTime;
                if (elapsed > 0x7fffffffUL) elapsed = (~elapsed) + 1UL;

                if (elapsed < 600000UL)    // predictions are only good for about 10-20 minutes from when holdover began
                {
                    int32_t delta = freqHistory[0] - freqHistory[freqHistoryLength-1];
                    int32_t prediction = (delta * (int32_t)elapsed + freqHistorySpanRounding) / freqHistorySpanMs;
                    freq_correction = freqHistory[0] + prediction;
                    nextHoldoverCorrection += 10000UL;
                    fracN->SetDivisor(freq_correction);
                }
                else
                {
                    freqHistoryCount = 0U; // reset to prevent any more corrections
                }
            }
        }
#endif
        return 0;
    }

    capture_flag = 0;
    //
    // at this point we have a PPS signal capture.
    //
    pps_timeout = now + 1500UL; // 1.5 second timeout on next PPS pulse
    //
    // if we're in holdover mode then see if there have been enough PPS captures
    // to be able to exit holdover mode.
    //
    if (holdoverCount > 0)
    {
        uint32_t pps = fracN->GetTotalTicks() + capture_time;

        if (prev_pps_valid)
        {
            int32_t delta = (int32_t)(pps - prev_pps_value) - 2000000L;
            if (labs(delta) < 2000L)
            {
                // pulses must be within 1ms of expected time to decrement holdover count
                holdoverCount--;
            }
            else
            {
                holdoverCount = 5;
                prev_pps_valid = 0U;
            }
        }
        else
        {
            prev_pps_valid = 1U;
        }

        prev_pps_value = pps;
        
        if (holdoverCount == 0) 
        {
            holdover_timed_out = 0U;
#ifdef HOLDOVER_PREDICTION
            holdoverStartTime = 0UL;
            freqHistoryTicker = -300;
            freqHistoryCount = 0U;
#endif
        }

        return 1U;
    }
    //
    // Get the captured PLL phase at the PPS edge. Phase is in units of time (ns).
    // The phase we get is always modulo 1-second since the PPS signal sets the period.
    //
    int32_t phase_error_ns = fracN->GetTimeNs(capture_time);
    //
    // force phase_error_ns to lie between +-1/2 second (+-500e6 ns)
    //
    while(phase_error_ns > 500000000L) phase_error_ns -= 1000000000L;
    //
    // update a counter that counts how many consecutive runs show phase error below 
    // some limit (1.5us). When this count reaches a threshold we signal that the 
    // loop is locked.
    //
    if (phase_error_ns > -1500L && phase_error_ns < 1500L)
    {
        if (settled < 60) settled++;
    }
    else
    {
        if (settled > 0) settled--;
    }

    int32_t phase_error_filtered = phase_error_ns;
    //
    // don't run any of the feedback loop calcs unless the current phase error
    // is less than one-half interrupt (1/64 second). the actual limit is about 2.4% bigger
    // to allow for a large initial frequency error
    //
    bool validPhase = (phase_error_ns >= -16000000L) && (phase_error_ns <= 16000000L);    

    if (!validPhase) 
    {
        //
        // if not in range, then shift our current count of interrupts so that we will be in range
        // for the next PPS interrupt.
        //
        fracN->SlewToCycle(2047);
        pllClockFilter.Clear();
        freqFirFilter.Clear();
        freqIirFilter.Clear();
        iirEnabled = 0;
        prev_pps_valid = 0;
        prev_pps_value = 0U;
        integrator = 0;
        pps_timeout = now + 2500UL;     // prevents false entry into holdover at power up; not sure why
        phase_initialized = 0;          // asks the PPS ISR to reset timer counts at next PPS pulse
#ifdef HOLDOVER_PREDICTION
        freqHistoryCount = 0U;
#endif
  
#if DEBUG_CSV
        uint32_t sec = fracN->GetNtpSeconds();
        debug_long(phase_error_ns);
        debug_str(","); debug_long(phase_error_filtered);
        debug_str(",0,0,0,0\r\n");
#endif
        return 1U;
    } 

    // 
    // ============ Begin feed-forward compensation calculations ===================
    //

    //
    // capture_time is the raw timer count when the PPS interrupt occurred 
    // GetTotalTicks() is the "MSW" of the time value and is incremented by the timer cycle count
    // each time the timer overflows. 
    // The sum, "pps" is the total number of timer ticks since time zero when the latest PPS was captured.
    //
    uint32_t pps = fracN->GetTotalTicks() + capture_time;
    //
    // pps now contains the total number of timer ticks since time zero; this value 
    // overflows every 35 minutes or so.
    //
    // the difference between successive ticks what we care about -- the derivative term.
    // in this case the targeted difference of 2e6 counts per PPS pulse is subtracted from the measured values,
    // so the filter sees only the difference from expected derivative value.
    //
    // finally a note on units. delta is in units of ticks per 32 timer cycles but the freq_correction term
    // must be in units of ticks per (32x64=2048) timer cycles. To get units correct, the filters, 
    // freqFirFilter and freqIirFilter have DC gains of 64 so their outputs do not require further scaling.
    //
    // careful...pps and prev_pps are unsigned but we want a signed result.
    //
    int32_t delta = (int32_t)(pps - prev_pps_value) - 2000000L;

    if (freqFirFilter.FillCount() > 0)
    {
        freq_correction = freqFirFilter.Step(delta);
        int32_t iir = freqIirFilter.Step(delta);
            
        // Serial.print(delta); Serial.print(","); Serial.print(freq_correction); Serial.print(","); Serial.println(iir);
            
        //
        // use IIR filtered frequency data if the filter is settled and agrees 
        // closely with the FIR filter output.
        //
        if (freqIirFilter.Filled()) 
        {
            if (labs(iir - freq_correction) < 25)
            {
                if (!iirEnabled) debug_str("! IIR\r\n");
                iirEnabled = 1U;
                freq_correction = iir;
            }
            else
            {
                freqIirFilter.Preset(delta);
                iirEnabled = 0U;
            }
        }
    }
    else
    {
        if (prev_pps_valid) // delta is not valid if prev_pps_value is zero
        {
            freqFirFilter.Preset(delta);
        }
        else
        {
            prev_pps_valid = 1U;
        }
    }

    prev_pps_value = pps;
        
    freq_correction = (freq_correction < -128000L) ? -128000L : 
        (freq_correction > 128000L ? 128000L : freq_correction);
        
#ifdef HOLDOVER_PREDICTION
    //
    // save current history once per minute for use in estimation of frequency ramp rate during holdover
    //
    if (freqHistoryTicker >= (int16_t)(freqHistoryInterval-1U))
    {
        memmove(freqHistory+1, freqHistory, sizeof(freqHistory)-sizeof(uint32_t));
        freqHistory[0] = freq_correction;
        lastFreqHistory = now; 
        if (freqHistoryCount < freqHistoryLength) freqHistoryCount++;
        freqHistoryTicker = 0U;
    }
    else
    {
        freqHistoryTicker++;
    }
#endif
    //
    // ============== End feed-forward calculations ==============
    //

    //
    // ============== Begin Phase-Locked Loop calculations ==============
    //

    //
    // don't bother filtering phase data unless the current phase error is less than 50us
    //
    if ((phase_error_ns >= -50000L) && (phase_error_ns <= 50000L))
    {
        int32_t y = pllClockFilter.Step(phase_error_ns);
        if (pllClockFilter.Filled()) phase_error_filtered = y;
    }
    else
    {
        //
        // if phase error exceeds some reasonable limit, stop filtering it until
        // it comes back into range; in fact, reset the filter's fill counter so 
        // filtering won't start until "N" samples after we have good data.
        //
        pllClockFilter.Clear();
    }
    // update the accumulated phase error term; by adding the filtered
    // pps time we are creating an integrator in the feedback loop.
    //
    integrator += phase_error_filtered;
    //
    // Compute the Proportional + Integral feedback term for the control loop
    //
    if (integrator >= PLL_INTEGRATOR_LIMIT)
    {
        integrator = PLL_INTEGRATOR_LIMIT;
    } 
    else 
    {
        if (integrator <= -PLL_INTEGRATOR_LIMIT)
        {
            integrator = 0L - PLL_INTEGRATOR_LIMIT;
        } 
    }
    //
    // this is the proportional term of the feedback loop;
    // the proportional gain is 1/PLL_PROPORTIONAL_DIV
    //
    phase_correction = phase_error_filtered / PLL_PROPORTIONAL_DIV;
    //
    // this is the integral term of the feedback loop;
    // the integral gain is 1/PLL_INTEGRAL_DIV
    //
    phase_correction += integrator / PLL_INTEGRAL_DIV; 
    //
    // limit total control effort 
    //
    if (phase_correction < -PLL_MAX_CTRL_EFFORT) 
    {
        phase_correction = -PLL_MAX_CTRL_EFFORT;
    }
    else
    {
        if (phase_correction > PLL_MAX_CTRL_EFFORT) phase_correction = PLL_MAX_CTRL_EFFORT;        
    }


#if DEBUG_CSV
    uint32_t sec = fracN->GetNtpSeconds();
    debug_long(phase_error_ns);
    debug_str(","); debug_long(phase_error_filtered);
    debug_str(","); debug_long(freq_correction);
    debug_str(","); debug_int(phase_correction);
    debug_str(","); debug_long(integrator);
    debug_str(","); debug_long(sec % 60);
    debug_str("\r\n");
#endif

    fracN->SetDivisor(freq_correction + phase_correction);

    return 1U;
}

#endif
