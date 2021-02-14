// 
// This file is a possibly modified version of the Ethernet2 library 
// distributed with some newer forks of Arduino (e.g. 1.7.7).
// It was derived from the Ethernet library that shipped with Arduino 1.6.5.
//
// Licensing of this library is a legal mess. This author did 
// not create the mess but attempts to deal with it herein.
//
// 1) www.arduino.cc states that all libraries (such as this one) 
//    are licensed under the LGPL.
// 2) Many files contain no license information at all.
// 3) Some files contain an MIT or Apache license declaration.
//
// Given the above morass, this author makes the following claims
// on licensing:
//
// 1) If the file is un-modified from the original version,
//    then whatever license applied is still in effect and
//    and the LGPL declaration below does not apply (unless the
//    original file was already under LGPL; good luck determining
//    if that is the case).
//
// 2) If the original file is modified and contained no licensing 
//    information, then it is licensed under LGPL as stated below.
//
// 3) If the original file is modified and contained an another license,
//    then it is still licensed under the other license and the 
//    LGPL declaration below does not apply.
//
// No licensing information has been removed from the original file.
//
// Copyright 2015 aweatherguy
// 
// This file is part of the ABSONS Ethernet2 library.
// ABSONS is an acronym for "Arduino-Based Stratum One Ntp Server".
// 
// The ABSONS Ethernet2 library is free software: you can redistribute 
// it and/or modify it under the terms of the GNU Lesser General Public 
// License as published by the Free Software Foundation, either 
// version 3 of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// 
// ====== End of added licensing notice. Any pre-existing notices are preserved below. ======
//

/*
  Twitter.cpp - Arduino library to Post messages to Twitter using OAuth.
  Copyright (c) NeoCat 2010-2011. All right reserved.
  
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 */

// ver1.2 - Use <string.h>
// ver1.3 - Support IDE 1.0

#include <string.h>
#include "TwitterNtp.h"

#define LIB_DOMAIN "arduino-tweet.appspot.com"

#if defined(ARDUINO) && ARDUINO < 100
static uint8_t server[] = {0,0,0,0}; // IP address of LIB_DOMAIN
Twitter::Twitter(const char *token) : client(server, 80), token(token)
{
}
#else
Twitter::Twitter(const char *token) : token(token)
{
}
#endif

bool Twitter::post(const char *msg)
{
#if defined(ARDUINO) && ARDUINO < 100
	DNSError err = EthernetDNS.resolveHostName(LIB_DOMAIN, server);
	if (err != DNSSuccess) {
		return false;
	}
#endif
	parseStatus = 0;
	statusCode = 0;
#if defined(ARDUINO) && ARDUINO < 100
	if (client.connect()) {
#else
	if (client.connect(LIB_DOMAIN, 80)) {
#endif
		client.println("POST http://" LIB_DOMAIN "/update HTTP/1.0");
		client.print("Content-Length: ");
		client.println(strlen(msg)+strlen(token)+14);
		client.println();
		client.print("token=");
		client.print(token);
		client.print("&status=");
		client.println(msg);
	} else {
		return false;
	}
	return true;
}

bool Twitter::checkStatus(Print *debug)
{
	if (!client.connected()) {
		if (debug)
			while(client.available())
				debug->print((char)client.read());
		client.flush();
		client.stop();
		return false;
	}
	if (!client.available())
		return true;
	char c = client.read();
	if (debug)
		debug->print(c);
	switch(parseStatus) {
	case 0:
		if (c == ' ') parseStatus++; break;  // skip "HTTP/1.1 "
	case 1:
		if (c >= '0' && c <= '9') {
			statusCode *= 10;
			statusCode += c - '0';
		} else {
			parseStatus++;
		}
	}
	return true;
}

int Twitter::wait(Print *debug)
{
	while (checkStatus(debug));
	return statusCode;
}
