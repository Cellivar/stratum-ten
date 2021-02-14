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
// 3) If the original file is modified and contained another license,
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

// Arduino DNS client for WizNet5100-based Ethernet shield
// (c) Copyright 2009-2010 MCQN Ltd.
// Released under Apache License, version 2.0

#ifndef DNSClient_h
#define DNSClient_h

#include <EthernetUdpNtp.h>

class DNSClient
{
public:
    // ctor
    void begin(const IPAddress& aDNSServer);

    /** Convert a numeric IP address string into a four-byte IP address.
        @param aIPAddrString IP address to convert
        @param aResult IPAddress structure to store the returned IP address
        @result 1 if aIPAddrString was successfully converted to an IP address,
                else error code
    */
    int inet_aton(const char *aIPAddrString, IPAddress& aResult);

    /** Resolve the given hostname to an IP address.
        @param aHostname Name to be resolved
        @param aResult IPAddress structure to store the returned IP address
        @result 1 if aIPAddrString was successfully converted to an IP address,
                else error code
    */
    int getHostByName(const char* aHostname, IPAddress& aResult);

protected:
    uint16_t BuildRequest(const char* aName);
    uint16_t ProcessResponse(uint16_t aTimeout, IPAddress& aAddress);

    IPAddress iDNSServer;
    uint16_t iRequestId;
    EthernetUDP iUdp;
};

#endif
