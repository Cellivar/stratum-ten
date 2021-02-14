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
#pragma once

static char NtpUtils_hexChars[16] = { 
            '0', '1', '2', '3', '4', '5', '6', '7',
            '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'  };

class NtpUtils
{
public:
    //
    // the standard Serial.print(uint32_t, HEX) function uses a divide
    // operation to determine print chars. Necessary for decimal but
    // very inefficient for HEX. This function is much more efficient,
    // and only prints in HEX. Remember, the long integer is stored in
    // little endian order.
    //
    static void printHex(uint8_t n, uint8_t newLine)
    {
        char val[3];
        val[0] = NtpUtils_hexChars[n >> 4];
        val[1] = NtpUtils_hexChars[n & 0x0f];
        val[2] = 0;
        newLine ? Serial.println(val) : Serial.print(val);
    }
    
    static void printHex(uint16_t n, uint8_t newLine)
    {
        char val[5];
        uint8_t* src = (uint8_t*)&n;

        char *s = val+3;
        uint8_t b;
        //
        // optimized for speed not memory so this is repeated 4 times, once for each byte
        //
        b = src[0];
        *s-- = NtpUtils_hexChars[b & 0x0f];
        *s-- = NtpUtils_hexChars[b >> 4];
        
        b = src[1];
        *s-- = NtpUtils_hexChars[b & 0x0f];
        *s   = NtpUtils_hexChars[b >> 4];

        val[4] = 0;

        newLine ? Serial.println(val) : Serial.print(val);
    }

    static void printHex(uint32_t n, uint8_t newLine)
    {
        char val[9];
        uint8_t* src = (uint8_t*)&n;

        char *s = val+7;
        uint8_t b;
        //
        // optimized for speed not memory so this is repeated 4 times, once for each byte
        //
        b = src[0];
        *s-- = NtpUtils_hexChars[b & 0x0f];
        *s-- = NtpUtils_hexChars[b >> 4];
        
        b = src[1];
        *s-- = NtpUtils_hexChars[b & 0x0f];
        *s-- = NtpUtils_hexChars[b >> 4];

        b = src[2];
        *s-- = NtpUtils_hexChars[b & 0x0f];
        *s-- = NtpUtils_hexChars[b >> 4];

        b = src[3];
        *s-- = NtpUtils_hexChars[b & 0x0f];
        *s = NtpUtils_hexChars[b >> 4];
        
        val[8] = 0;

        newLine ? Serial.println(val) : Serial.print(val);
    }
};

