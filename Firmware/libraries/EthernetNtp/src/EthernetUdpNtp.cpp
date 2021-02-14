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
 *  Udp.cpp: Library to send/receive UDP packets with the Arduino ethernet shield.
 *  This version only offers minimal wrapping of socket.c/socket.h
 *  Drop Udp.h/.cpp into the Ethernet library directory at hardware/libraries/Ethernet/ 
 *
 * MIT License:
 * Copyright (c) 2008 Bjoern Hartmann
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * bjoern@cs.stanford.edu 12/30/2008
 *
 * - 10 Apr. 2015
 * Added support for Arduino Ethernet Shield 2
 * by Arduino.org team
 */

#define NTP_SERVER 1

#include "utility/w5500.h"
#include "utility/socket.h"
#include "EthernetNtp.h"
#include "Udp.h"
#include "DnsNtp.h"

/* Constructor */
EthernetUDP::EthernetUDP() : _sock(MAX_SOCK_NUM) {}

/* Start EthernetUDP socket, listening at local port PORT */
uint8_t EthernetUDP::begin(uint16_t port) {
  if (_sock != MAX_SOCK_NUM)
    return 0;

  for (int i = 0; i < MAX_SOCK_NUM; i++) {
    uint8_t s = w5500.readSnSR(i);
    if (s == SnSR::CLOSED || s == SnSR::FIN_WAIT) {
      _sock = i;
      break;
    }
  }

  if (_sock == MAX_SOCK_NUM)
    return 0;

  _port = port;
  _remaining = 0;
  socket(_sock, SnMR::UDP, _port, 0);

  // TODO: setup interrupts for NTP app here...

  return 1;
}

/* return number of bytes available in the current packet,
   will return zero if parsePacket hasn't been called yet */
int EthernetUDP::available() {
  return _remaining;
}

/* Release any resources being used by this EthernetUDP instance */
void EthernetUDP::stop()
{
  if (_sock == MAX_SOCK_NUM)
    return;

  close(_sock);

  EthernetClass::_server_port[_sock] = 0;
  _sock = MAX_SOCK_NUM;
}

int EthernetUDP::beginPacket(const char *host, uint16_t port)
{
  // Look up the host first
  int ret = 0;
  DNSClient dns;
  IPAddress remote_addr;

  dns.begin(Ethernet.dnsServerIP());
  ret = dns.getHostByName(host, remote_addr);
  if (ret == 1) {
    return beginPacket(remote_addr, port);
  } else {
    return ret;
  }
}

int EthernetUDP::beginPacket(IPAddress ip, uint16_t port)
{
  _offset = 0;
  return startUDP(_sock, rawIPAddress(ip), port);
}

int EthernetUDP::endPacket()
{
  return sendUDP(_sock);
}

size_t EthernetUDP::write(uint8_t byte)
{
  return write(&byte, 1);
}

size_t EthernetUDP::write(const uint8_t *buffer, size_t size)
{
  uint16_t bytes_written = bufferData(_sock, _offset, buffer, size);
  _offset += bytes_written;
  return bytes_written;
}

int EthernetUDP::parsePacket()
{
  // discard any remaining bytes in the last packet
  flush();

  if (w5500.getRXReceivedSize(_sock) > 0)
  {
    //HACK - hand-parse the UDP packet using TCP recv method
    uint8_t tmpBuf[8];
    int ret =0; 
    //read 8 header bytes and get IP and port from it
    ret = recv(_sock,tmpBuf,8);
    if (ret > 0)
    {
      _remoteIP = tmpBuf;
      _remotePort = tmpBuf[4];
      _remotePort = (_remotePort << 8) + tmpBuf[5];
      _remaining = tmpBuf[6];
      _remaining = (_remaining << 8) + tmpBuf[7];

      // When we get here, any remaining bytes are the data
      ret = _remaining;
    }
    return ret;
  }
  // There aren't any packets available
  return 0;
}

int EthernetUDP::read()
{
  uint8_t byte;

  if ((_remaining > 0) && (recv(_sock, &byte, 1) > 0))
  {
    // We read things without any problems
    _remaining--;
#ifdef NTP_SERVER
    if (_remaining == 0) w5500.writeSnIR(_sock, SnIR::RECV); 
#endif
    return byte;
  }

  // If we get here, there's no data available
  return -1;
}

int EthernetUDP::read(unsigned char* buffer, size_t len)
{

  if (_remaining > 0)
  {

    int got;

    if (_remaining <= len)
    {
      // data should fit in the buffer
      got = recv(_sock, buffer, _remaining);
    }
    else
    {
      // too much data for the buffer, 
      // grab as much as will fit
      got = recv(_sock, buffer, len);
    }

    if (got > 0)
    {
      _remaining -= got;
#ifdef NTP_SERVER
      if (_remaining == 0) w5500.writeSnIR(_sock, SnIR::RECV); 
#endif
      return got;
    }

  }

  // If we get here, there's no data available or recv failed
  return -1;

}

int EthernetUDP::peek()
{
  uint8_t b;
  // Unlike recv, peek doesn't check to see if there's any data available, so we must.
  // If the user hasn't called parsePacket yet then return nothing otherwise they
  // may get the UDP header
  if (!_remaining)
    return -1;
  ::peek(_sock, &b);
  return b;
}

void EthernetUDP::flush()
{
  // could this fail (loop endlessly) if _remaining > 0 and recv in read fails?
  // should only occur if recv fails after telling us the data is there, lets
  // hope the w5500 always behaves :)
    if (_remaining)
    {
      while (_remaining)
      {
        read();
      }
    
#ifdef NTP_SERVER
      //
      // clear the socket's RECV interrupt but only if there was remaining data
      //
      if (_remaining == 0) w5500.writeSnIR(_sock, SnIR::RECV); 
#endif

    }
}

