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

#include "NtpFractionalN.h"
#include "NtpHdwr.h"

NtpFractionalN::NtpFractionalN() : FractionalN(11, NOMINAL_INTEGER_DIVISOR)
{
    ntpSecondsValid = 0U;
    ntpSecondsOffset = 0UL;

    ntpCapturedTimeOffset = PLL_OFFSET_NTP;    
    ntpCurrentTimeOffset  = PLL_OFFSET_NTP;    
}

NtpFractionalN::~NtpFractionalN()
{
}

uint8_t NtpFractionalN::UnservicedInterruptCount()
{
    return InterruptCount();
}

void NtpFractionalN::TimerMatchIsr()
{
    //
    // if there's a pending UDP packet capture (ICR5) then save that data first
    //
    if (UDP_CAPTURE_FLAG) 
    {
        memcpy((void*)&udpCapture, (void*)&state, sizeof(FracNState));

        udpCapture.c = UDP_CAPTURE_COUNT;
        udpCapture.flags = 1U;

        RESET_UDP_CAPTURE_FLAG; 
    }
    //
    // now let the base class ISR run
    //
    FractionalN::TimerMatchIsr();
}

void NtpFractionalN::SetCaptureOffsetNtp(int32_t Offset) { clientCapturedTimeOffset = Offset; }

void NtpFractionalN::SetCurrentOffsetNtp(int32_t Offset) { clientCurrentTimeOffset = Offset; }

uint8_t NtpFractionalN::NtpSecondsValid()
{
    return ntpSecondsOffset != 0UL;
}

uint8_t NtpFractionalN::SetNtpSeconds(uint32_t Seconds)
{
    //
    // get current count of seconds
    //
    uint32_t t = (state.subCycle >> 5) + (state.cycle << 6);
    //
    // set the ntp seconds offset so sum will be correct
    // NOTE: this might or might not fail after the next NTP era begins...can we fix that? or is it okay?
    //
    uint8_t prevValid = ntpSecondsValid;
    uint32_t prevOffset = ntpSecondsOffset;
    
    ntpSecondsOffset = Seconds - t;
    ntpSecondsValid = 1U;

    if (!prevValid || (ntpSecondsOffset != prevOffset)) 
    {
        Serial.print("! Set NTP sec = "); Serial.println(Seconds); 
    }
    //
    // a false return value means that our previous tracking of ntp seconds had become incorrect.
    //
    return (!prevValid) || (ntpSecondsOffset == prevOffset);
}

uint32_t NtpFractionalN::GetNtpSeconds()
{
    return (state.subCycle >> 5) + (state.cycle << 6) + ntpSecondsOffset;
}

uint32_t NtpFractionalN::GetTimeNs(uint16_t TimerCount)
{
    state.c = TimerCount;
    int32_t ns = ComputePhase((pFracNState)&state, NOM_TICK_NS, NOM_CYCLE_NS);
    ns += (state.subCycle & 0x1fU) * NOM_CYCLE_NS;
    ns += PLL_OFFSET_NS;
    while (ns > 1000000000L) ns -= 1000000000L;
    return ns;
}

void NtpFractionalN::GetCurrentNtpTime(uint16_t *TimerCount, uint32_t *Seconds, uint32_t *Fraction)
{
    state.c = FRAC_N_COUNT;

#if ENABLE_TIMESTAMP_DEBUG_FLAG
    PORTH |= 0x20; // turn on debug flag indicating we've captured current time
    // if things changed and the loop repeats there will be a short pulse followed by
    // the flag going on and staying on
#endif

    *TimerCount = state.c;
    int32_t offset = ntpCurrentTimeOffset + clientCurrentTimeOffset;

    NtpTime((pFracNState)&state, offset, Seconds, Fraction);
}

void NtpFractionalN::ResetCapturedNtpTime()
{
    udpCapture.flags &= ~0x01U;
    RESET_UDP_CAPTURE_FLAG;
}

uint8_t NtpFractionalN::GetCapturedNtpTime(uint32_t *Seconds, uint32_t *Fraction)
{
    uint8_t ok = 0;

    if (udpCapture.flags & 0x01U)
    {
        ok = 1;
    }
    else
    {
        ok = (UDP_CAPTURE_FLAG != 0);
        memcpy((void*)&udpCapture, (void*)&state, sizeof(FracNState));
        udpCapture.c = UDP_CAPTURE_COUNT;
    }
    
    udpCapture.flags &= ~0x01U;
    RESET_UDP_CAPTURE_FLAG; 
    
    int32_t offset = ntpCapturedTimeOffset + clientCapturedTimeOffset;

    NtpTime((pFracNState)&udpCapture, offset, Seconds, Fraction);

    return ok;
}

void NtpFractionalN::NtpTime(pFracNState fState, int32_t Offset, uint32_t *Seconds, uint32_t *Fraction)
{
    uint32_t frac = ComputePhase(fState, NOM_TICK_NTP, NOM_CYCLE_NTP);

    frac += NOM_CYCLE_NTP * (fState->subCycle & 0x1fU);
    //
    // this addition of Offset is typically 1/2 NOM_CYCLE_NS and can roll-over in 32-bit math
    // if that happens we know because the sum will be less than the original value.
    // when rollover occurs, we increment the integer seconds count to account for it.
    //
    uint32_t offsetFrac = frac + Offset;

    uint32_t sec = (fState->subCycle >> 5) + (fState->cycle << 6) + ntpSecondsOffset;
    if (offsetFrac < frac) sec++;

    *Seconds = sec;
    *Fraction = offsetFrac;
}

#endif
