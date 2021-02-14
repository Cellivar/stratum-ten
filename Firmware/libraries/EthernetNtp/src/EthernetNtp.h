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
// 3) Some files contain an MIT license declaration.
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
// 3) If the original file is modified and contained an MIT license,
//    then it is still licensed under the MIT license and the 
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
 modified 12 Aug 2013
 by Soohwan Kim (suhwan@wiznet.co.kr)

 - 10 Apr. 2015
 Added support for Arduino Ethernet Shield 2
 by Arduino.org team
 
 */
#ifndef ethernet_h
#define ethernet_h

#include <inttypes.h>
#include "utility/w5500.h"
#include "IPAddress.h"
#include "EthernetClientNtp.h"
#include "EthernetServerNtp.h"
#include "DhcpNtp.h"



class EthernetClass {
private:
  IPAddress _dnsServerAddress;
  DhcpClass* _dhcp;
public:
  static uint8_t _state[MAX_SOCK_NUM];
  static uint16_t _server_port[MAX_SOCK_NUM];

#if defined(WIZ550io_WITH_MACADDRESS)
  // Initialize function when use the ioShield serise (included WIZ550io)
  // WIZ550io has a MAC address which is written after reset.
  // Default IP, Gateway and subnet address are also writen.
  // so, It needs some initial time. please refer WIZ550io Datasheet in details.
  int begin(void);
  void begin(IPAddress local_ip);
  void begin(IPAddress local_ip, IPAddress dns_server);
  void begin(IPAddress local_ip, IPAddress dns_server, IPAddress gateway);
  void begin(IPAddress local_ip, IPAddress dns_server, IPAddress gateway, IPAddress subnet);
#else
  // Initialize the Ethernet shield to use the provided MAC address and gain the rest of the
  // configuration through DHCP.
  // Returns 0 if the DHCP configuration failed, and 1 if it succeeded
  int begin(uint8_t *mac_address);
  void begin(uint8_t *mac_address, IPAddress local_ip);
  void begin(uint8_t *mac_address, IPAddress local_ip, IPAddress dns_server);
  void begin(uint8_t *mac_address, IPAddress local_ip, IPAddress dns_server, IPAddress gateway);
  void begin(uint8_t *mac_address, IPAddress local_ip, IPAddress dns_server, IPAddress gateway, IPAddress subnet);

#endif
  
  int maintain();

  IPAddress localIP();
  IPAddress subnetMask();
  IPAddress gatewayIP();
  IPAddress dnsServerIP();

  friend class EthernetClient;
  friend class EthernetServer;
};

extern EthernetClass Ethernet;

#endif
