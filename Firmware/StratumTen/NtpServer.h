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

#include <EthernetNtp.h>
#include <EthernetUdpNtp.h>

#include "NtpConfig.h"
#include "Timekeeping.h"

class NtpServer
{

public:
    
    NtpServer( Timekeeping *FracN, EthernetUDP *Udp );

    uint8_t ProcessRequest(uint8_t *Packet, uint16_t Length);

    size_t PacketSize();

    void UpdateAssociations();  // clear timed-out entries from the peer state table

    //
    // Pending has 3 recognized values; others are ignored.
    // 0    no leap second pending
    // 1    add second at end of day
    // 0xff subtract second at end of day
    //
    // This determines the leap second warning bits in the NTP packet headers that we send
    //
    void SetLeapSecondStatus(uint8_t Pending);

private:
    uint32_t ChangeEndians(uint32_t n);
    
    const uint16_t NTP_PORT = 123;

    const uint8_t NTP_LI_OK      = 0;
    const uint8_t NTP_LI_ADD_SEC = 1;
    const uint8_t NTP_LI_SUB_SEC = 2;
    const uint8_t NTP_LI_ALARM   = 3;
    
    uint8_t leapSecondStatus = NTP_LI_ALARM;        // must be one of the NTP_LI_xxx constants

    const uint8_t NTP_MODE_RESERVED     = 0;
    const uint8_t NTP_MODE_SYM_ACTIVE   = 1;
    const uint8_t NTP_MODE_SYM_PASSIVE  = 2;
    const uint8_t NTP_MODE_CLIENT       = 3;
    const uint8_t NTP_MODE_SERVER       = 4;    // this is the mode to set in reply packets
    const uint8_t NTP_MODE_BROADCAST    = 5;
    const uint8_t NTP_MODE_CONTROL      = 6;
    const uint8_t NTP_MODE_PRIVATE      = 7;

    const uint8_t NTP_VERSION  = 4;
    const char    NTP_REF_ID[4] = { 'G', 'P', 'S', 0 };

    const uint8_t NO_PEER =         0xffU;    // value returned by FindPeer on failure
    //
    // flags for packet timestamp validity tests
    // currently, lower nibble bits do not prevent response packets being sent
    //
    const uint8_t FLAG_UNSYNC =     0x01U;
    const uint8_t FLAG_BOGUS =      0x02U;
    const uint8_t FLAG_DUPLICATE =  0x10U;
    const uint8_t FLAG_INVALID =    0x20U;

    const uint8_t FLAGS_ABORT =     0xf0U;   // these bits will abort a response packet
    
    //
    // format of the 48-byte NTP message
    //
    #pragma pack 1
    typedef struct {
        //
        // first byte contains three fields
        //
        uint8_t     Flags;

        uint8_t     Stratum;
        int8_t      Poll;                   // log2 of maximum polling interval
        int8_t      Precision;              // log2 of local clock precision

        int32_t     RootDelay;              // total roundtrip delay to reference clock
        int32_t     RootDispersion;         // maximum error relative to reference clock
        char        ReferenceId[4];         // appropriate for stratum-1 servers, e.g. "GPS\0"

        uint32_t ReferenceTimestamp[2];
        uint32_t OriginateTimestamp[2];
        uint32_t ReceiveTimestamp[2];
        uint32_t TransmitTimestamp[2];

    } NtpPacket, *pNtpPacket;
    
    const uint16_t NTP_PACKET_SIZE = sizeof(NtpPacket);

    Timekeeping *fracN;
    EthernetUDP *udp;

    // const uint32_t ZeroTimestamp[2] = { 0UL, 0UL };
    //
    // tries to find the peer in table.
    // - if found, returns table index
    // - if not found adds peer to table and returns index (might include deleting a timed out table entry)
    // - if table is full returns 0xff
    //
    uint8_t FindPeer(uint32_t Address);  // Address is peer's IP address
    //
    // call this periodically to check for peer entry timeouts
    //
    void UpdatePeers();
    //
    // there are two flags in here -- XlMode and NextIsBogus. There's no reason those can't be two bits
    // in a single byte. There's an extra byte available for now so that's easier. If this struct needs
    // to track additional data, those two values can be combined into a single byte.
    //
    #pragma pack 1
    typedef struct {
        uint32_t Ip;                    // key is first for convenience; zero means the entry is empty
        uint8_t  XlMode;                // non-zero when in interleaved symmetric mode bit zero is only used for now
        uint8_t  NextIsBogus;           // used only in interleaved mode to flag 2nd packet of an invalid pair
        uint16_t SlotTimeout;           // when TIMEDOUT(millis(), SlotTimeout << 16) this entry can be re-used
        uint32_t BasicRndId[2];         // round ID for basic mode = tx timestamp sent in previous packet
        uint32_t XlRndId[2];            // round ID for interleaved mode = rx timestamp sent in previous packet
        uint32_t HardTx[2];
        uint32_t PrevTx[2];             // last tx timestamp received; used to detect duplicate packets
    } PeerState, *pPeerState;

    PeerState symmetricPeers[ MAX_SYMMETRIC_PEER_COUNT ];
    
    uint8_t ValidateSymmetricTimestamps(pNtpPacket packet, pPeerState peer);
    
    void InitializePeer(pPeerState Peer, uint32_t Address);

    void InitializePacketHeader(pNtpPacket Packet, uint8_t Mode);

    void PrintPacket(pNtpPacket Packet);
};
