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

#include "utility/w5500.h"
#include "utility/socket.h"
extern "C" {
#include "string.h"
}

#include "EthernetNtp.h"
#include "EthernetClientNtp.h"
#include "EthernetServerNtp.h"

EthernetServer::EthernetServer(uint16_t port)
{
  _port = port;
}

void EthernetServer::begin()
{
  for (int sock = 0; sock < MAX_SOCK_NUM; sock++) {
    EthernetClient client(sock);
    if (client.status() == SnSR::CLOSED) {
      socket(sock, SnMR::TCP, _port, 0);
      listen(sock);
      EthernetClass::_server_port[sock] = _port;
      break;
    }
  }  
}

void EthernetServer::accept()
{
  int listening = 0;

  for (int sock = 0; sock < MAX_SOCK_NUM; sock++) {
    EthernetClient client(sock);

    if (EthernetClass::_server_port[sock] == _port) {
      if (client.status() == SnSR::LISTEN) {
        listening = 1;
      } 
      else if (client.status() == SnSR::CLOSE_WAIT && !client.available()) {
        client.stop();
      }
    } 
  }

  if (!listening) {
    begin();
  }
}

EthernetClient EthernetServer::available()
{
  accept();

  for (int sock = 0; sock < MAX_SOCK_NUM; sock++) {
    EthernetClient client(sock);
    if (EthernetClass::_server_port[sock] == _port &&
        (client.status() == SnSR::ESTABLISHED ||
         client.status() == SnSR::CLOSE_WAIT)) {
      if (client.available()) {
        // XXX: don't always pick the lowest numbered socket.
        return client;
      }
    }
  }

  return EthernetClient(MAX_SOCK_NUM);
}

size_t EthernetServer::write(uint8_t b) 
{
  return write(&b, 1);
}

size_t EthernetServer::write(const uint8_t *buffer, size_t size) 
{
  size_t n = 0;
  
  accept();

  for (int sock = 0; sock < MAX_SOCK_NUM; sock++) {
    EthernetClient client(sock);

    if (EthernetClass::_server_port[sock] == _port &&
      client.status() == SnSR::ESTABLISHED) {
      n += client.write(buffer, size);
    }
  }
  
  return n;
}
