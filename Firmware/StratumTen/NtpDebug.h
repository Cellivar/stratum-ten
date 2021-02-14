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
#include <inttypes.h>
//
// If the Serial port (i.e. USB COM port) is not connected to a host then the
// Serial output buffer will fill up and calls to Serial.print() will hang waiting
// for the buffer to empty. To prevent this we test for space in the buffer prior
// to writing the information
//
#define debug_str(x)    if (Serial.availableForWrite() > strlen(x)) Serial.print(x)
#define debug_int(x)    if (Serial.availableForWrite() > 8) Serial.print(x)
#define debug_long(x)   if (Serial.availableForWrite() > 12) Serial.print(x)
