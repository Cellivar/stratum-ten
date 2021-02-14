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
 * - 10 Apr. 2015
 * Added support for Arduino Ethernet Shield 2
 * by Arduino.org team
 */
 
#define NTP_SERVER 1

#if NTP_SERVER

#define ENABLE_TRANSMIT_INTERRUPTS 1

#define W5500_INTERRUPT_PORT PORTL
#define W5500_INTERRUPT_MASK 0x02U

#endif

#include "utility/w5500.h"
#include "utility/socket.h"

static uint16_t local_port;

/**
 * @brief	This Socket function initialize the channel in perticular mode, and set the port and wait for w5500 done it.
 * @return 	1 for success else 0.
 */
uint8_t socket(SOCKET s, uint8_t protocol, uint16_t port, uint8_t flag)
{
  if ((protocol == SnMR::TCP) || (protocol == SnMR::UDP) || (protocol == SnMR::IPRAW) || (protocol == SnMR::MACRAW) || (protocol == SnMR::PPPOE))
  {
    close(s);
    w5500.writeSnMR(s, protocol | flag);
    if (port != 0) {
      w5500.writeSnPORT(s, port);
    } 
    else {
      local_port++; // if don't set the source port, set local_port number.
      w5500.writeSnPORT(s, local_port);
    }

    w5500.execCmdSn(s, Sock_OPEN);
    
#ifdef NTP_SERVER
      //
      // enable true hardware interrupt on socket RECV events
      // for debug, try enabling interrupts for write complete too
      //
#if ENABLE_TRANSMIT_INTERRUPTS
      w5500.writeSnIR(s, SnIR::RECV | SnIR::SEND_OK);   // clear any pending interrupts
      w5500.writeSnIMR(s, SnIR::RECV | SnIR::SEND_OK);
#else
      w5500.writeSnIR(s, SnIR::RECV);   // clear any pending interrupts
      w5500.writeSnIMR(s, SnIR::RECV);
#endif
      w5500.writeSIR(1 << s);           // clear any pending socket interrupts
      w5500.writeSIMR(w5500.readSIMR() | (1 << s));
#endif

  return 1;
  }

  return 0;
}


/**
 * @brief	This function close the socket and parameter is "s" which represent the socket number
 */
void close(SOCKET s)
{
  w5500.execCmdSn(s, Sock_CLOSE);       // shut down the socket

#ifdef NTP_SERVER
  w5500.writeSnIMR(s, 0);               // turn off all interrupts in the socket's IMR

  uint8_t imr = w5500.readSIR() & (~(1 << s));
  w5500.writeSIR(imr);                  // turn of interrupts for the closed socket
#endif

  w5500.writeSnIR(s, 0xFF);             // clear all interrupts in the socket
}


/**
 * @brief	This function established  the connection for the channel in passive (server) mode. This function waits for the request from the peer.
 * @return	1 for success else 0.
 */
uint8_t listen(SOCKET s)
{
  if (w5500.readSnSR(s) != SnSR::INIT)
    return 0;
  w5500.execCmdSn(s, Sock_LISTEN);
  return 1;
}


/**
 * @brief	This function established  the connection for the channel in Active (client) mode. 
 * 		This function waits for the untill the connection is established.
 * 		
 * @return	1 for success else 0.
 */
uint8_t connect(SOCKET s, uint8_t * addr, uint16_t port)
{
  if 
    (
  ((addr[0] == 0xFF) && (addr[1] == 0xFF) && (addr[2] == 0xFF) && (addr[3] == 0xFF)) ||
    ((addr[0] == 0x00) && (addr[1] == 0x00) && (addr[2] == 0x00) && (addr[3] == 0x00)) ||
    (port == 0x00) 
    ) 
    return 0;

  // set destination IP
  w5500.writeSnDIPR(s, addr);
  w5500.writeSnDPORT(s, port);
  w5500.execCmdSn(s, Sock_CONNECT);

  return 1;
}



/**
 * @brief	This function used for disconnect the socket and parameter is "s" which represent the socket number
 * @return	1 for success else 0.
 */
void disconnect(SOCKET s)
{
  w5500.execCmdSn(s, Sock_DISCON);
}


/**
 * @brief	This function used to send the data in TCP mode
 * @return	1 for success else 0.
 */
uint16_t send(SOCKET s, const uint8_t * buf, uint16_t len)
{
  uint8_t status=0;
  uint16_t ret=0;
  uint16_t freesize=0;

  if (len > w5500.SSIZE) 
    ret = w5500.SSIZE; // check size not to exceed MAX size.
  else 
    ret = len;

  // if freebuf is available, start.
  do 
  {
    freesize = w5500.getTXFreeSize(s);
    status = w5500.readSnSR(s);
    if ((status != SnSR::ESTABLISHED) && (status != SnSR::CLOSE_WAIT))
    {
      ret = 0; 
      break;
    }
  } 
  while (freesize < ret);

  // copy data
  w5500.send_data_processing(s, (uint8_t *)buf, ret);
  w5500.execCmdSn(s, Sock_SEND);

  /* +2008.01 bj */
  while ( (w5500.readSnIR(s) & SnIR::SEND_OK) != SnIR::SEND_OK ) 
  {
    /* m2008.01 [bj] : reduce code */
    if ( w5500.readSnSR(s) == SnSR::CLOSED )
    {
      close(s);
      return 0;
    }
  }
  /* +2008.01 bj */
  w5500.writeSnIR(s, SnIR::SEND_OK);
  return ret;
}


/**
 * @brief	This function is an application I/F function which is used to receive the data in TCP mode.
 * 		It continues to wait for data as much as the application wants to receive.
 * 		
 * @return	received data size for success else -1.
 */
int16_t recv(SOCKET s, uint8_t *buf, int16_t len)
{
  // Check how much data is available
  int16_t ret = w5500.getRXReceivedSize(s);
  if ( ret == 0 )
  {
    // No data available.
    uint8_t status = w5500.readSnSR(s);
    if ( status == SnSR::LISTEN || status == SnSR::CLOSED || status == SnSR::CLOSE_WAIT )
    {
      // The remote end has closed its side of the connection, so this is the eof state
      ret = 0;
    }
    else
    {
      // The connection is still up, but there's no data waiting to be read
      ret = -1;
    }
  }
  else if (ret > len)
  {
    ret = len;
  }

  if ( ret > 0 )
  {
    w5500.recv_data_processing(s, buf, ret);
    w5500.execCmdSn(s, Sock_RECV);
  }
  return ret;
}


/**
 * @brief	Returns the first byte in the receive queue (no checking)
 * 		
 * @return
 */
uint16_t peek(SOCKET s, uint8_t *buf)
{
  w5500.recv_data_processing(s, buf, 1, 1);

  return 1;
}


/**
 * @brief	This function is an application I/F function which is used to send the data for other then TCP mode. 
 * 		Unlike TCP transmission, The peer's destination address and the port is needed.
 * 		
 * @return	This function return send data size for success else -1.
 */
uint16_t sendto(SOCKET s, const uint8_t *buf, uint16_t len, uint8_t *addr, uint16_t port)
{
  uint16_t ret=0;

  if (len > w5500.SSIZE) ret = w5500.SSIZE; // check size not to exceed MAX size.
  else ret = len;

  if
    (
  ((addr[0] == 0x00) && (addr[1] == 0x00) && (addr[2] == 0x00) && (addr[3] == 0x00)) ||
    ((port == 0x00)) ||(ret == 0)
    ) 
  {
    /* +2008.01 [bj] : added return value */
    ret = 0;
  }
  else
  {
    w5500.writeSnDIPR(s, addr);
    w5500.writeSnDPORT(s, port);

    // copy data
    w5500.send_data_processing(s, (uint8_t *)buf, ret);
    w5500.execCmdSn(s, Sock_SEND);

    /* +2008.01 bj */
    while ( (w5500.readSnIR(s) & SnIR::SEND_OK) != SnIR::SEND_OK ) 
    {
      if (w5500.readSnIR(s) & SnIR::TIMEOUT)
      {
        /* +2008.01 [bj]: clear interrupt */
        w5500.writeSnIR(s, (SnIR::SEND_OK | SnIR::TIMEOUT)); /* clear SEND_OK & TIMEOUT */
        return 0;
      }
    }

    /* +2008.01 bj */
    w5500.writeSnIR(s, SnIR::SEND_OK);
  }
  return ret;
}


/**
 * @brief	This function is an application I/F function which is used to receive the data in other then
 * 	TCP mode. This function is used to receive UDP, IP_RAW and MAC_RAW mode, and handle the header as well. 
 * 	
 * @return	This function return received data size for success else -1.
 */
uint16_t recvfrom(SOCKET s, uint8_t *buf, uint16_t len, uint8_t *addr, uint16_t *port)
{
  uint8_t head[8];
  uint16_t data_len=0;
  uint16_t ptr=0;

  if ( len > 0 )
  {
    ptr = w5500.readSnRX_RD(s);
    switch (w5500.readSnMR(s) & 0x07)
    {
    case SnMR::UDP :
      w5500.read_data(s, ptr, head, 0x08);
      ptr += 8;
      // read peer's IP address, port number.
      addr[0] = head[0];
      addr[1] = head[1];
      addr[2] = head[2];
      addr[3] = head[3];
      *port = head[4];
      *port = (*port << 8) + head[5];
      data_len = head[6];
      data_len = (data_len << 8) + head[7];

      w5500.read_data(s, ptr, buf, data_len); // data copy.
      ptr += data_len;

      w5500.writeSnRX_RD(s, ptr);
      break;

    case SnMR::IPRAW :
      w5500.read_data(s, ptr, head, 0x06);
      ptr += 6;

      addr[0] = head[0];
      addr[1] = head[1];
      addr[2] = head[2];
      addr[3] = head[3];
      data_len = head[4];
      data_len = (data_len << 8) + head[5];

      w5500.read_data(s, ptr, buf, data_len); // data copy.
      ptr += data_len;

      w5500.writeSnRX_RD(s, ptr);
      break;

    case SnMR::MACRAW:
      w5500.read_data(s, ptr, head, 2);
      ptr+=2;
      data_len = head[0];
      data_len = (data_len<<8) + head[1] - 2;

      w5500.read_data(s, ptr, buf, data_len);
      ptr += data_len;
      w5500.writeSnRX_RD(s, ptr);
      break;

    default :
      break;
    }
    w5500.execCmdSn(s, Sock_RECV);
  }
  return data_len;
}

/**
 * @brief	Wait for buffered transmission to complete.
 */
void flush(SOCKET s) {
  // TODO
}

uint16_t igmpsend(SOCKET s, const uint8_t * buf, uint16_t len)
{
  uint8_t status=0;
  uint16_t ret=0;

  if (len > w5500.SSIZE) 
    ret = w5500.SSIZE; // check size not to exceed MAX size.
  else 
    ret = len;

  if (ret == 0)
    return 0;

  w5500.send_data_processing(s, (uint8_t *)buf, ret);
  w5500.execCmdSn(s, Sock_SEND);

  while ( (w5500.readSnIR(s) & SnIR::SEND_OK) != SnIR::SEND_OK ) 
  {
    status = w5500.readSnSR(s);
    if (w5500.readSnIR(s) & SnIR::TIMEOUT)
    {
      /* in case of igmp, if send fails, then socket closed */
      /* if you want change, remove this code. */
      close(s);
      return 0;
    }
  }

  w5500.writeSnIR(s, SnIR::SEND_OK);

  return ret;
}

uint16_t bufferData(SOCKET s, uint16_t offset, const uint8_t* buf, uint16_t len)
{
  uint16_t ret =0;
  if (len > w5500.getTXFreeSize(s))
  {
    ret = w5500.getTXFreeSize(s); // check size not to exceed MAX size.
  }
  else
  {
    ret = len;
  }
  w5500.send_data_processing_offset(s, offset, buf, ret);
  return ret;
}

int startUDP(SOCKET s, uint8_t* addr, uint16_t port)
{
  if
    (
     ((addr[0] == 0x00) && (addr[1] == 0x00) && (addr[2] == 0x00) && (addr[3] == 0x00)) ||
     ((port == 0x00))
    ) 
  {
    return 0;
  }
  else
  {
    w5500.writeSnDIPR(s, addr);
    w5500.writeSnDPORT(s, port);
    return 1;
  }
}

int sendUDP(SOCKET s)
{
  w5500.execCmdSn(s, Sock_SEND);

#ifdef NTP_SERVER
#if ENABLE_TRANSMIT_INTERRUPTS
  //
  // SEND_OK interrupts are enabled for the NTP server option so wait for an interrupt before doing 
  // furious reads on the IR register looking for the SEND_OK/TIMEOUT events
  // Timer0 may be disabled so don't count on using that for any kind of timeout
  // Normally, the 48-byte UDP packet will successfully send in much less than 200us
  // Two exceptional events that may occur are 
  // 1) Timeout .... in this case the loop times out and we'll see the timeout status but below
  // 2) Another UDP packet is received (RECV interrupt) ... in this case we'll bail early
  //    but will still wait for SEND_OK or TIMEOUT in the while loop below.
  //
  // loop timing: here is the disassembly with normal flow cycle counts in the first column
  //
  // 1      ldi	r18, 0xF4	; 244   ; 500 LSB
  // 1      ldi	r19, 0x01	; 1     ; 500 MSB
  // 2      lds	r24, 0x010B         ; load PORTH
  // 2      sbrs	r24, 1          ; skip if bit set (normal flow is to skip the rjmp)
  // 0      rjmp	.+0      	    ; 0x1e <_Z7sendUDPh+0x1e>   ; jump to exit (not counted since normally skipped)
  // 1      subi	r18, 0x01	    ; 1 ; decrement k
  // 1      sbc	r19, r1             ; decrement k
  // 2      brne	.+0      	    ; 0x24 <_Z7sendUDPh+0x24>   ; repeat loop
  // ===============================================
  // 10 total cycles 
  //
  // n = 200us x F_CPU / 10 = F_CPU / 50,000 
  // Note: the compiler will compute the integer division and it won't require any CPU cycles.
  // This change seems to save about 25usec in the execution of ::endPacket(). Perhaps not worth
  // it and this extra wait loop can be removed w/o causing any problems.
  //
  // several experiments show that the W5500 typically asserts the interrupt 29.1us after
  // the call is made to sendUDP, and about 5.5us after the last SPI transaction finishes.
  // this seems to imply that the packet transmission starts almost immediately after the SPI commands are sent.
  //
  const uint16_t n = (uint16_t)(F_CPU / 50000UL);

  for (uint16_t k=0; k<n; k++)
  {
      if ((W5500_INTERRUPT_PORT & W5500_INTERRUPT_MASK) == 0) break;
  }  
#endif
#endif

  while ( (w5500.readSnIR(s) & SnIR::SEND_OK) != SnIR::SEND_OK ) 
  {
    if (w5500.readSnIR(s) & SnIR::TIMEOUT)
    {
      /* +2008.01 [bj]: clear interrupt */
      w5500.writeSnIR(s, (SnIR::SEND_OK|SnIR::TIMEOUT));
      return 0;
    }
  }

  /* +2008.01 bj */	
  w5500.writeSnIR(s, SnIR::SEND_OK);

  /* Sent ok */
  return 1;
}

