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

#include "NtpServer.h"
#include "NtpConfig.h"
#include "NtpHdwr.h"
#include "NtpUtils.h"

#define TS_IS_ZERO(timestamp) ((timestamp[0] == 0UL) && (timestamp[1] == 0UL))

#define TS_EQUAL(t1, t2) ((t1[0] == t2[0]) && (t1[1] == t2[1]))

#define CLEAR_TS(timestamp) timestamp[0] = 0UL; timestamp[1] = 0UL;

#define MEASURE_TX_DELAY 0

NtpServer::NtpServer( Timekeeping *FracN, EthernetUDP *Udp )
{
    fracN = FracN;
    udp = Udp;
    memset(symmetricPeers, 0, MAX_SYMMETRIC_PEER_COUNT * sizeof(PeerState));
}

size_t NtpServer::PacketSize() { return NTP_PACKET_SIZE; }

void NtpServer::SetLeapSecondStatus(uint8_t Pending)
{
    switch (Pending)
    {
    case 0:
        leapSecondStatus = NTP_LI_OK;
        break;
    case 1U:
        leapSecondStatus = NTP_LI_ADD_SEC;
        break;
    case 0xffU:
        leapSecondStatus = NTP_LI_SUB_SEC;
        break;
    default:
        break;
    }
    // Serial.print("! Leap "); NtpUtils::printHex(leapSecondStatus, 1U);
}

uint32_t NtpServer::ChangeEndians(uint32_t n)
{
    uint32_t m;
    uint8_t *np = (uint8_t*)&n;
    uint8_t *mp = (uint8_t*)&m;
    mp[0] = np[3];
    mp[1] = np[2];
    mp[2] = np[1];
    mp[3] = np[0];
    return m;
}
//
// The current implementation tracks invalid round IDs (aka bogus packets)
// but does not do anything with that information. That's because we don't need
// to calculate offset/delay w.r.t. the peer's clock. If we did, then that information
// would be necessary.
//
uint8_t NtpServer::ValidateSymmetricTimestamps(pNtpPacket packet, pPeerState peer)
{
    uint8_t rc = 0U;

    if (peer->NextIsBogus)
    {
        rc = FLAG_BOGUS;
        peer->NextIsBogus = 0U;
        Serial.println("! NB");
    }

    if (TS_IS_ZERO(packet->TransmitTimestamp))
    {
        // synchronization packet...
        return rc | FLAG_UNSYNC;
        Serial.println("! US");
    }

    if (TS_EQUAL(packet->TransmitTimestamp, peer->PrevTx) && (! TS_IS_ZERO(peer->PrevTx)))
    {
        // duplicate packet...ignore it
        return rc | FLAG_DUPLICATE;
    }

    if ((peer->XlMode & 0x01U) == 0U)
    {
        // see if the origin timestamp equals the current basic round id
        if (TS_EQUAL(packet->OriginateTimestamp, peer->BasicRndId))
        {
            CLEAR_TS(peer->BasicRndId);
            return rc;
        }
        else
        {
            //
            // if the packet would be valid in interleaved mode then switch to that
            // in any case this is a bogus packet.
            //
            if (TS_EQUAL(packet->OriginateTimestamp, peer->XlRndId))
            {
                peer->XlMode |= 0x01U;
            }
            return rc | FLAG_BOGUS;
        }
    }
    //
    // if we get here we are in interleaved symmetric mode (or at least think we are)
    // check for an unsynchronized condition
    //
    if (TS_IS_ZERO(packet->OriginateTimestamp) || 
        TS_IS_ZERO(packet->ReceiveTimestamp) ||
        TS_IS_ZERO(peer->XlRndId))
    {
        return rc | FLAG_UNSYNC;
    }
    //
    // check for valid interleaved round ID
    //
    if (! TS_EQUAL(packet->OriginateTimestamp, peer->XlRndId))
    {
        peer->NextIsBogus = 1U;
        return rc | FLAG_BOGUS;
    }

    return rc;
}

void NtpServer::InitializePacketHeader(pNtpPacket Packet, uint8_t Mode)
{
    Packet->Flags = (leapSecondStatus << 6) | (NTP_VERSION << 3) | Mode;
    Packet->Stratum = 1;
    memcpy(Packet->ReferenceId, NTP_REF_ID, sizeof(Packet->ReferenceId));
    Packet->Poll = 4;         // modern computers work best with fastest poll -- 16 seconds
    Packet->Precision = -21;  // 2^-21 sec = 477ns
    Packet->RootDelay = 0;
    Packet->RootDispersion = 0x01000000; // in big-endian this is 0x00000001 = smallest allowed value = 2^-16 second?    
}

uint8_t NtpServer::ProcessRequest(uint8_t *Packet, uint16_t Length)
{
    if (Length < NTP_PACKET_SIZE) 
    {
        Serial.print("Bad NTP request length "); Serial.println(Length);
        return 0U;
    }

    pNtpPacket ntpPacket = (pNtpPacket)Packet;

    uint8_t leapIndicator, version, mode;
    uint8_t pidx, rc;
    uint32_t rxSecs, rxFrac;
    uint32_t txSecs, txFrac, adjTxFrac;
    uint32_t originalRxStamp[2];
    uint32_t now, timeout;
    uint16_t timeoutT5;
    pPeerState peer;

#if MEASURE_TX_DELAY
    uint16_t tx1, tx2;
#endif
    
    //
    // get capture time of incoming UDP packet
    //
    uint8_t haveCaptureEvent = fracN->GetCapturedNtpTime(&rxSecs, &rxFrac);
    //
    // parse and validate packet header
    //
    leapIndicator = ntpPacket->Flags >> 6;
    version = (ntpPacket->Flags >> 3) & 0x07;
    mode = ntpPacket->Flags & 0x07;

    if (mode != NTP_MODE_SYM_ACTIVE && mode != NTP_MODE_CLIENT) 
    {
        //Serial.println("! Mode");
        return 0U;
    }
    if (version < 3)
    {
        //Serial.println("! Version");
        return 0U;
    }
    //
    // get the IP address of the client requesting service
    //
    IPAddress ntpRequestor = udp->remoteIP();
    
    //Serial.print("! PKT "); Serial.print(ntpRequestor);
    //Serial.print(", v"); Serial.println(version);

    if (!haveCaptureEvent) 
    {
        //Serial.println("! Capture");
        return 0U;
    }

    //
    // copy captured timestamp into outgoing reply packet in all cases
    // but save original; we'll need it if in interleaved symmetric mode...
    //
    memcpy(originalRxStamp, ntpPacket->ReceiveTimestamp, sizeof(originalRxStamp));

    ntpPacket->ReceiveTimestamp[0] = ChangeEndians(rxSecs);
    ntpPacket->ReceiveTimestamp[1] = ChangeEndians(rxFrac);

    memcpy(ntpPacket->ReferenceTimestamp, ntpPacket->ReceiveTimestamp, sizeof(ntpPacket->ReferenceTimestamp));

    if (mode == NTP_MODE_SYM_ACTIVE)
    {
        // Serial.println("! XL");
        pidx = FindPeer((uint32_t)ntpRequestor);        
        if (pidx == NO_PEER) 
        {
            // Serial.println("! No Peer");
            return 0U;
        }

        // Serial.print("! P "); NtpUtils::printHex(pidx, 0U);
        peer = symmetricPeers + pidx;
        // Serial.print(","); NtpUtils::printHex(peer->XlMode, 0U);

        rc = ValidateSymmetricTimestamps(ntpPacket, peer);
        // Serial.print(","); NtpUtils::printHex(rc, 1U);

        if (rc & FLAGS_ABORT) 
        {
            // Serial.print("! XX "); NtpUtils::printHex(rc, 1U);
            return 0U;
        }
        //
        // although we did not abort, the packet could still be unsynchronized or bogus,
        // but we put together a response anyway in these cases.
        //

        //
        // update copy of last tx TS received so we can detect new duplicates
        //
        memcpy(peer->PrevTx, ntpPacket->TransmitTimestamp, sizeof(peer->PrevTx));

        InitializePacketHeader(ntpPacket, NTP_MODE_SYM_ACTIVE);

        if (peer->XlMode)
        {
            // PrintPacket(ntpPacket);
            //
            // this is more straightforward...just prepare the packet and send it.
            // then copy the transmit hardstamp into peer state and we're done.
            //
            memcpy(ntpPacket->OriginateTimestamp, originalRxStamp, sizeof(ntpPacket->OriginateTimestamp));
            memcpy(ntpPacket->TransmitTimestamp, peer->HardTx, sizeof(ntpPacket->OriginateTimestamp));
            //
            // set the timestamp we expect to see in the originate field of the next packet from peer
            // also set the value that we would get if the peer were running in basic mode
            //
            memcpy(peer->XlRndId, ntpPacket->ReceiveTimestamp, sizeof(peer->XlRndId));
            memcpy(peer->BasicRndId, ntpPacket->TransmitTimestamp, sizeof(peer->BasicRndId));
            //
            // copy packet into NIC and send
            //
            udp->beginPacket(ntpRequestor, NTP_PORT);
            udp->write(Packet, NTP_PACKET_SIZE);
            // RESET_UDP_CAPTURE_FLAG;
            udp->endPacket();
            //
            // the send is complete now, grab the transmit hardstamp
            //
            haveCaptureEvent = fracN->GetCapturedNtpTime(&txSecs, &txFrac);
            if (haveCaptureEvent)
            {
                peer->HardTx[0] = ChangeEndians(txSecs);
                peer->HardTx[1] = ChangeEndians(txFrac);
                if (1)
                {
                    //Serial.println("! TX");
                    //Serial.print("! TX "); NtpUtils::printHex(txSecs, 0U);
                    //Serial.print(","); NtpUtils::printHex(txFrac, 1U);
                }
            }
            else
            {
                Serial.println("! TX-");
                // yikes...transmit must have failed...? just zero the hardstamp
                CLEAR_TS(peer->HardTx);
                return 0U;
            }
            return 1U;
        }
    }
    else
    {
        InitializePacketHeader(ntpPacket, NTP_MODE_SERVER);
    }
    //
    // this code works for both basic symmetric and server modes
    //
    // copy transmit timestamp into originate timestamp; this serves to identify both
    // server mode and basic symmetric mode packets.
    //
    memcpy(ntpPacket->OriginateTimestamp, ntpPacket->TransmitTimestamp, sizeof(ntpPacket->OriginateTimestamp));
    //
    // get a transmit timestamp, stuff it into the packet and transmit after appropriate delay
    //
    udp->beginPacket(ntpRequestor, NTP_PORT);
    uint16_t mark;
    //
    // this call will turn on PORTH bit 5 (digital 8) to indicate the capture of current time
    //
    fracN->GetCurrentNtpTime(&mark, &txSecs, &txFrac);
    //
    // add a fixed amount of delay to the timestamp -- longer than it will take to get the NIC loaded 
    // with the reply and ready to transmit. 
    // Then we'll wait until that delay has expired and kick off the transmission of the UDP packet.
    //
    uint8_t carry = 0U;
    adjTxFrac = txFrac + NTP_TX_ADJUST_NTP;
    if (adjTxFrac < txFrac)
    {
        txSecs++;
        carry = 1U;
    }
    
    ntpPacket->TransmitTimestamp[0] = ChangeEndians(txSecs);
    ntpPacket->TransmitTimestamp[1] = ChangeEndians(adjTxFrac);

    udp->write(Packet, NTP_PACKET_SIZE);

    uint16_t rollover = NOMINAL_INTEGER_DIVISOR - mark; // how many ticks until the timer rolls over?
    uint16_t xmitTimeout;
    //
    // turn off Timer0 from now until after the tranmission has been kicked off
    // this code is processor-specific but the sketch is targeted only to the Mega1280/2560 and
    // This code may be incorrect for other Arduino boards/Atmel processors.
    //
    uint8_t t0cs = TCCR0B & TCCR0B_CS_MASK;  // save current clock rate bits so we can restore later
    TCCR0B &= ~TCCR0B_CS_MASK;
    //
    // turn off debug flag indicating we're beginning the wait for proper transmit time.
    // it was turned on in fracN->GetCurrentNtpTime()...see DisciplinedTimers.cpp for implementation
    //
#if ENABLE_TIMESTAMP_DEBUG_FLAG
    PORTH &= ~0x20; 
#endif    
    //
    // Begin time-critical section of code. It is important that the execution time from
    // timeout expiration to packet transmission does not vary due to interrupts or 
    // excessively variable code branching. That's why each code branch below ends with
    // a very similar while() loop. We want no branching to occur after timer reaches target value.
    // as a possible improvement, the while() loops could be writtine in assembly...that may or 
    // may not improve things.
    //
    if (NTP_TX_DELAY_TICKS >= rollover)
    {
        xmitTimeout = NTP_TX_DELAY_TICKS - rollover;
        if (xmitTimeout <= 2U)
        {
            //
            // when timer rolls over, we're done;
            //
            while (FRAC_N_COUNT > xmitTimeout);
        }
        else
        {
            //
            // wait for rollover, then wait for desired count
            //
            while (FRAC_N_COUNT > xmitTimeout);
            while (FRAC_N_COUNT < xmitTimeout);
        }
    }
    else
    {
        xmitTimeout = mark + NTP_TX_DELAY_TICKS;
        while (FRAC_N_COUNT < xmitTimeout);
    }
#if MEASURE_TX_DELAY
    tx1 = FRAC_N_COUNT;
#endif
    //
    // start transmission of packet after the controlled time interval relative
    // to the time we read the timer
    //
#if ENABLE_TIMESTAMP_DEBUG_FLAG
    PORTH |= 0x20; // turn on transmit flag indicating we're about to kick off transmission
#endif

    udp->endPacket(); // this will begin transmission of the packet and does not return until completed.

#if ENABLE_TIMESTAMP_DEBUG_FLAG
    PORTH &= ~0x20; // turn off transmit flag indicating that packet has been kicked off (at some level?)
#endif
    //
    // start Timer0 running again (restore clock select bits to whatever they were before
    //
    TCCR0B |= t0cs;
    //
    // how long did the tx operation require, including network access delays?
    //
#if MEASURE_TX_DELAY
    tx2 = UDP_CAPTURE_COUNT - tx1;
    Serial.print("! Tx "); 
    NtpUtils::printHex(tx2, 1U);
#endif
    //
    // reset any UDP interrupts...they may have arrived while processing this request
    // or there may be spurious interrupts caused by SEND_OK being enabled.
    //
    RESET_UDP_CAPTURE_FLAG;

    if (mode == NTP_MODE_SYM_ACTIVE) 
    {
        //
        // update both basic and interleaved round IDs. Even though we're in basic mode
        // here, that will allow us to detect a peer operating in interleaved mode and
        // switch modes to match.
        //
        memcpy(peer->BasicRndId, ntpPacket->TransmitTimestamp, sizeof(peer->BasicRndId));
        memcpy(peer->XlRndId, ntpPacket->ReceiveTimestamp, sizeof(peer->XlRndId));
    }
    //
    // enable this to print the transmit timestamp in hex
    //
#if 0
    if (carry) Serial.print("+");
    NtpUtils::printHex(txSecs, 0); Serial.print(".");
    NtpUtils::printHex(adjTxFrac, 1);
#endif
    //
    // un-comment this line for transmit timestamp calibration. it prints the time between
    // Rx and Tx timestamps. Units are fractional seconds -- divide by 2^32 to get seconds
    // This can also be measured on a scope looking at the W5500 interrupt pin. Compare the
    // printed value to scope reading and adjust NTP_TX_FINE_ADJUST_NS in NtpConfig... file
    // to make them match.
    //

    // NtpUtils::printHex(adjTxFrac-rxFrac,1);

    return 1U;
}

void NtpServer::InitializePeer(pPeerState Peer, uint32_t Address)
{
    //
    // re-zero entire struct then init fields of interest. cleaner and easier this way.
    // peer entries are valid for about an hour w/o refreshing
    //
    memset(Peer, 0, sizeof(PeerState));
    Peer->Ip = Address;
    Peer->SlotTimeout = (uint16_t)(millis() >> 16) + 60U;
    //
    // in order to support both basic and interleaved symmetric modes this must be
    // initialized to zero. Setting to 1 as we do here removes support for the 
    // basic symmetric mode. there's no performance advantage in that mode compared
    // to client/server so it is not really necessary to support it.
    //
    Peer->XlMode = 1U;
}

void NtpServer::UpdateAssociations()
{
    uint8_t k;
    uint32_t now = millis();
    uint32_t timeout;

    for (k=0; k<MAX_SYMMETRIC_PEER_COUNT; k++)
    {
        if (symmetricPeers[k].Ip == 0UL) continue;
        timeout = ((uint32_t)symmetricPeers[k].SlotTimeout) << 16;
        if (TIMEDOUT(now, timeout))
        {
            symmetricPeers[k].Ip = 0UL;     // delete current peer, make slot available.
        }
    }
}

//
// tries to find the peer in symmetricPeers[] table.
// - if found, returns table index
// - if not found, attempts to add peer to table and either:
//   * returns index, or
//   * returns 0xff if table is full
//
uint8_t NtpServer::FindPeer(uint32_t Addr)
{
    if (Addr == 0UL) return 0xff;  // invalid address
    
    uint8_t k;

    for (k=0; k<MAX_SYMMETRIC_PEER_COUNT; k++)
    {
        if (symmetricPeers[k].Ip == Addr) return k;
    }
    //
    // find an empty slot
    //
    for (k=0; k<MAX_SYMMETRIC_PEER_COUNT; k++)
    {
        if (symmetricPeers[k].Ip == 0UL)
        {
            InitializePeer(symmetricPeers + k, Addr);
            return k;
        }
    }
    //
    // look for a timed-out slot
    //
    uint32_t now = millis();

    for (k=0; k<MAX_SYMMETRIC_PEER_COUNT; k++)
    {
        uint32_t t = (uint32_t)(symmetricPeers[k].SlotTimeout) << 16;
        if (TIMEDOUT(now, t))
        {
            InitializePeer(symmetricPeers + k, Addr);
            return k;
        }
    }

    return NO_PEER;
}

void NtpServer::PrintPacket(pNtpPacket Packet)
{
    Serial.print("! Torg "); NtpUtils::printHex(ChangeEndians(Packet->OriginateTimestamp[0]), 0U);
    Serial.print(","); NtpUtils::printHex(ChangeEndians(Packet->OriginateTimestamp[1]), 1U);
    
    Serial.print("! Trcv "); NtpUtils::printHex(ChangeEndians(Packet->ReceiveTimestamp[0]), 0U);
    Serial.print(","); NtpUtils::printHex(ChangeEndians(Packet->ReceiveTimestamp[1]), 1U);

    Serial.print("! Txmt "); NtpUtils::printHex(ChangeEndians(Packet->TransmitTimestamp[0]), 0U);
    Serial.print(","); NtpUtils::printHex(ChangeEndians(Packet->TransmitTimestamp[1]), 1U);

}
