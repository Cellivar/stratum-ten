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

#include <Arduino.h>
#include "Led.h"


Led::Led(int Pin)
{
    pin = Pin;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    onTime = 250U;
    offTime = 250U;
    flashCount = 0U;
    flashContinuous = 0U;
    staticOn = 0U;
}

void Led::ConfigureFlash(uint16_t OnTimeMs, uint16_t OffTimeMs)
{
    onTime = OnTimeMs;
    offTime = OffTimeMs;
}

void Led::Set(uint8_t On)
{
    digitalWrite(pin, On ? HIGH : LOW);
    flashCount = 0U;
    flashContinuous = 0U;
    staticOn = On ? 1U : 0U;
}

uint8_t Led::Get()
{
    return digitalRead(pin) == HIGH;
}

void Led::On()
{
    digitalWrite(pin, HIGH);
    flashCount = 0U;            // cancel remaining flashes
    flashContinuous = 0U;
    staticOn = 1U;
}

void Led::Off()
{
    digitalWrite(pin, LOW);
    flashCount = 0U;            // cancel remaining flashes
    flashContinuous = 0U;
    staticOn = 0U;
}

void Led::Flash(uint8_t Count)
{    
    flashContinuous = 0U;
    if (flashCount == 0U) 
    {
        timeout = millis() + onTime;
        digitalWrite(pin, staticOn ? LOW : HIGH);
    }
    flashCount += Count << 1U;
}

void Led::FlashContinuous()
{
    if (flashContinuous) return;  // I heard you the first time!
    flashCount = 0U;
    flashContinuous = 0x02;
    timeout = millis() + onTime;
    digitalWrite(pin, HIGH);
}

uint8_t Led::Busy() 
{ 
    return flashContinuous || (flashCount != 0) ; 
}

uint8_t Led::Update()
{
    uint32_t now;

    if (flashContinuous)
    {
        now = millis();

        if (now >= timeout)
        {
            flashContinuous ^= 0x01;
            if (flashContinuous & 0x01)
            {
                digitalWrite(pin, staticOn ? HIGH : LOW); 
                timeout = now + offTime;
            }
            else
            {
                digitalWrite(pin, staticOn ? LOW : HIGH); 
                timeout = now + onTime;
            }
        }
        return 1U;
    }

    if (flashCount == 0) return 0U;

    now = millis();

    if (now >= timeout)
    {
        if (--flashCount)
        {
            if (flashCount & 0x01)
            {
                digitalWrite(pin, staticOn ? HIGH : LOW);
                timeout = now + offTime;
            }
            else
            {
                digitalWrite(pin, staticOn ? LOW : HIGH);
                timeout = now + onTime;
            }
        }
    }

    return flashCount != 0U;
}

