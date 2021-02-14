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
// Credits:
// 
// The author of this software wishes to thank Andrew Rodland of Madison, WI
// for ideas presented on the following internet web page:
//     http://cleverdomain.org/yapc2012/
// and in software made available at the following URL:
//     http://git.cleverdomain.org/arduino/
// 
// =========================================================================
// 
// This sketch is designed specifically to run on the Arduino Mega2560 version 3 board
// It will probably run on older Mega2560 and even Mega1280's but none of that is 
// guaranteed. Your mileage may vary.
//
// This version is tested only with the Mega using a 10MHz disciplined clock supplied
// by the GPS receiver/reference clock. There must always be EXACTLY 10,000,000 clock
// cycles for every PPS pulse. If the GPS has a "jam-sync" option which "jumps" the 
// PPS signal on-time in certain situations that must be disabled.
//
// See NtpConfig.h for most of the tunable parameters.
// 
#include <Arduino.h>
#include <SPI.h>
#include "EthernetNtp.h"
#include "EthernetUdpNtp.h"

#include "NtpConfig.h"

#include "NtpHdwr.h"
#include "NtpDebug.h"
#include "NtpUtils.h"
#include "Timekeeping.h"
#include "DisciplinedTimers.h"
#include "NtpFractionalN.h"
#include "NtpPhaseLockedLoop.h"
#include "HP55300A.h"
#include "IcmSmt360.h"
#include "NtpServer.h"
#include "Led.h"

#ifdef HAVE_RESET_FLAGS
//
// this will only work with Mega if the bootloader has been modified to place
// initial MCUSR contents into r2 before starting the application.
// the default stk500v2 boot loader does NOT do this and must be enhanced for 
// that purpose.
//
uint8_t resetFlags __attribute__ ((section(".noinit")));
void resetFlagsInit(void) __attribute__ ((naked)) __attribute__ ((section (".init0")));
void resetFlagsInit(void)
{
  // save the reset flags passed from the bootloader
  __asm__ __volatile__ ("mov %0, r2\n" : "=r" (resetFlags) :);
}
#endif

//
// =============== Ethernet variables ===================
//
byte mac[] = { NTP_SERVER_MAC };

IPAddress myIpAddr, myDnsIpAddr, mySubnetMask, myGatewayIpAddr;

const uint16_t NTP_PORT = 123;

EthernetUDP Udp;
//
// generic class pointer to NTP time-keeping functions such as
// getting and setting NTP time from the GPS clock and maintaining
// current time. For non-disciplined clocks the instance will be the phase-locked loop class,
// while for disciplined clocks it will be the DisciplinedTimers class.
//
Timekeeping *keepTime;
//
// timer cycle count range during which  background I/O with GPS hardware is permitted
//
#define TOD_CYCLE_MIN 2
#define TOD_CYCLE_MAX (NOM_CYCLE_PER_SEC - 3)
//
// range of cycles in which we will reply to NTP requests
// that fall within the working window
//
#define NTP_CYCLE_MIN 1
#define NTP_CYCLE_MAX (NOM_CYCLE_PER_SEC - 2)

#if SYSCLK_DISCIPLINED == 0

NtpFractionalN fracN = NtpFractionalN();
NtpPhaseLockedLoop PLL = NtpPhaseLockedLoop(&fracN);

#else

DisciplinedTimers goodTimers = DisciplinedTimers();

#endif
//
// ====================== variables associated with timers 4 and 5 ========================
//
uint8_t timersRunning;       // true if we've started timers running (need GPS locked prior to timer start)
int timeCheck = 0;          // how many valid PPS signals have we received
uint8_t timerCycles = 0;    // how many overflow cycles out of a 1-second period have elapsed from last PPS ?
uint32_t ntpWindowTimeout = 0UL;       // expiration of working time window for ntp requests
//
// class instances to work with GPS ref clock hardware
//
#if GPS_IS_HP55300A
HP55300A HpGps = HP55300A(&Serial1);
SerialGps *TOD = &HpGps;
#endif

#if GPS_IS_ICM_SMT_360
IcmSmt360 TrimbleGps = IcmSmt360(&Serial1);
SerialGps *TOD = &TrimbleGps;
#endif
//
// class to perform NTP server functions via UDP ethernet packets
//
NtpServer *ntpServer; // = NtpServer(&fracN, &Udp);
size_t NtpPacketSize;
uint8_t *NtpPacket;

uint8_t timeOfDayValid = 0U;
uint32_t timeOfDayValidTimeout = 0UL;                   // causes TOD to go invalid if not updated frequently
const uint32_t timeOfDayValidTimeLimit = 3600000UL;    // one hour, in milliseconds

uint8_t pllLock = 0U;
uint8_t holdover = 0U;
uint32_t errorTimeout;
uint8_t errorLedEnabled = 0U;

#if SYSCLK_DISCIPLINED

Led ledError = Led(LED_ERROR);
Led ledHoldover = Led(LED_HOLDOVER);
Led ledLock = Led(LED_GPS_LOCK);
Led ledPps = Led(LED_PPS);
Led ledRqst = Led(LED_RQST);

#else

Led ledError = Led(LED_ERROR);
Led ledTod = Led(LED_TOD_OK);
Led ledLock = Led(LED_PLL_LOCK);
Led ledPps = Led(LED_PPS);
Led ledRqst = Led(LED_RQST);

#endif

//
// ISR's are here because they need to call methods on classes
// which have been declared in this file.
//
ISR(TIMER4_COMPA_vect) 
{
#if SYSCLK_DISCIPLINED
    goodTimers.TimerMatchIsr();
#else
    fracN.TimerMatchIsr();
#endif
}

ISR(TIMER4_CAPT_vect) 
{
#if SYSCLK_DISCIPLINED
    goodTimers.PpsCaptureIsr();
#else
    PLL.PhaseCaptureIsr();  
#endif
}

void setup() 
{
    InitializeHardware();
    timersRunning = 0U;    
    
#if SYSCLK_DISCIPLINED == 0
    keepTime = &fracN;
#else
    keepTime = &goodTimers;
#endif
    //
    // cable delay is specified in NtpConfig.h
    //
    keepTime->SetCaptureOffsetNtp(ANTENNA_CABLE_DELAY_NTP);
    keepTime->SetCurrentOffsetNtp(ANTENNA_CABLE_DELAY_NTP);

    ntpServer = new NtpServer(keepTime, &Udp);


    Serial.begin(57600);  
    Serial.println("! Stratum-1 NTP Server");

    Serial.print("! Version "); Serial.print(FIRMWARE_MAJOR);
    Serial.print("."); Serial.print(FIRMWARE_MINOR);
    Serial.print("."); Serial.println(FIRMWARE_UPDATE);

#if SYSCLK_DISCIPLINED
    Serial.println("! Using GPS-disciplined 10MHz processor clock.");
#else
    Serial.println("! Using software PLL and fractional-N clock management.");
#endif

#ifdef HOLDOVER_PREDICTION
    Serial.print("! Holdover prediction enabled\r\n");
#endif
    //
    // Initialize GPS/ref clock hardware interfaces
    //
#if GPS_IS_HP55300A
    Serial.print("! Using HP55300A Reference Clock\r\n");
    Serial1.begin(9600, SERIAL_8N1);
#endif
    
#if GPS_IS_ICM_SMT_360

    Serial.print("! Using Trimble ICM SMT 360 Reference Clock\r\n");
    //
    // With a 10MHz system clock, the HardwareSerial.begin() method will choose
    // a value of 0x0A for the UBRR1 register in double-speed mode, resulting in
    // an actual baud rate of 10e6/(11 x 8) = 113,636 which differs from the desired
    // rate by -1.36%. A lucky coincidence, this should be close enough.
    // Over ten bit periods (one start bit, 8 data bits and one stop bit), 
    // the clock will be off by 14% which should be okay.
    //
    Serial1.begin(115200, SERIAL_8O1);
#endif
    
    delay(100);
    TOD->FlushInput();

#if GPS_IS_HP55300A
    Serial1.print("*cls\n");
    while (Serial1.available()) 
    {
        //int c = Serial1.read();
        //Serial.print((char)c);
    }
    //Serial.println();
#endif
    //
    // get the ethernet interface up and running
    //
    NtpPacketSize = ntpServer->PacketSize();
    NtpPacket = new uint8_t[NtpPacketSize];

    myIpAddr = IPAddress( NTP_SERVER_IP ); // 192,168,1,111);
    myDnsIpAddr = IPAddress(192,168,1,1);
    mySubnetMask = IPAddress(255,255,255,0);
    myGatewayIpAddr = IPAddress(192,168,1,1);
    //
    // As an NTP server we probably want to use a hard IP address
    //
#if F_CPU <= 12000000UL
    //
    // double clock rate for 10MHz system clock...will run at 5MHz on SPI then.
    // will the SPI library will alter this setting in it's begin() function?
    //
    SPCR &= ~0x03; SPSR |= 0x01;
#endif

    Ethernet.begin(mac, myIpAddr);  // this one works on local LAN but not WLAN...?
    // Ethernet.begin(mac, myIpAddr, myDnsIpAddr, myGatewayIpAddr, mySubnetMask);
    Udp.begin(NTP_PORT);
    
#if F_CPU <= 12000000UL
    //
    // do it again in case the two begin() calls above have altered the SPI clock rate.
    //
    SPCR &= ~0x03; SPSR |= 0x01;
#endif

    Serial.print("! Ethernet started, MAC");
    for (int k=0; k<sizeof(mac); k++) 
    {
        Serial.print(" ");
        NtpUtils::printHex(mac[k], 0U);
    }
    Serial.println();
    Serial.print("! UDP socket open at ");
    Serial.println(myIpAddr); 
    //
    // flash the LEDs cyclicly eight times to indicate we are alive
    //
#if SYSCLK_DISCIPLINED
    ledError.Flash(8U);  delay(40);
    ledHoldover.Flash(8U);    delay(40);
    ledLock.Flash(8U);   delay(40);
    ledPps.Flash(8U);    delay(40);
    ledRqst.Flash(8U);

    while (ledRqst.Busy())
    {
        ledError.Update();
        ledHoldover.Update();
        ledLock.Update();
        ledPps.Update();
        ledRqst.Update();
    }    
#else
    ledError.Flash(8U);  delay(40);
    ledTod.Flash(8U);    delay(40);
    ledLock.Flash(8U);   delay(40);
    ledPps.Flash(8U);    delay(40);
    ledRqst.Flash(8U);

    while (ledRqst.Busy())
    {
        ledError.Update();
        ledTod.Update();
        ledLock.Update();
        ledPps.Update();
        ledRqst.Update();
    }
    
#endif
    //
    // the error LED is flashed quickly every time an NTP request is ignored
    // configure the duration here:
    //
    ledError.ConfigureFlash(100U, 100U);

    delay(1000UL);

    ledPps.Flash(FIRMWARE_MAJOR);
    while (ledPps.Update());
    delay(1500UL);

    ledPps.Flash(FIRMWARE_MINOR);
    while (ledPps.Update());
    delay(1500UL);

    ledPps.Flash(FIRMWARE_UPDATE);
    while (ledPps.Update());
    delay(1500UL);

#if SYSCLK_DISCIPLINED
#else
    PLL.Init();
#endif
    //
    // 13 minutes for all to get settled before error light can go on
    // it can take up to 12 minutes to receive UTC offset from the satellites
    //
    errorTimeout        = millis() + 780000UL;         
    //
    // we've been busy flashing LEDs so the serial input buffer from GPS may have overflowed
    // if it is broadcasting data/packets. Flush the input before starting so that important
    // messages are not lost.
    //
    TOD->FlushInput();
#if GPS_IS_ICM_SMT_360
    //
    // this gps needs to know if we want a new position survey.
    // to avoid excessive surveying, we only request that if we are in setup() because
    // of a power-on reset. however, boot loader changes are required for us to be able
    // to determine that, so for now we always ask for a survey. doesn't seem to hurt 
    // anything...?
    //
    TrimbleGps.Init(1U);
#else
    TOD->Init();    // initialize GPS/ref clock hardware
#endif
}
//
// checks if time of day and GPS status is valid and attempts to acquire fresh data periodically
// the "periodically" part is managed by the TOD's GetTimeOfDay() method.
//
uint8_t checkTOD()
{
    if (timeOfDayValid && (TIMEDOUT(millis(), timeOfDayValidTimeout))) 
    {
        Serial.println("! TOD timeout");
        timeOfDayValid = 0U;
    }

    uint8_t rc = TOD->Step();

    if (rc)
    {
        // we got something, could be TOD or some status information; check for TOD first.
        uint32_t tod = TOD->GetTimeOfDay();

        if (tod != 0UL)
        {
            if (!TOD->Holdover())
            {
                if (timeOfDayValid == 1U) Serial.println("! TOD valid");
                if (keepTime->SetNtpSeconds(tod))
                {
                    if (timeOfDayValid < 0xffU) timeOfDayValid++;
                }
                timeOfDayValidTimeout = millis() + timeOfDayValidTimeLimit;
            }
        }

        if (TOD->AllConditionsNormal())
        {
            ntpServer->SetLeapSecondStatus(TOD->LeapSecondPending());
        }

        return 1U;
    }
    return 0U;
}

void loop() 
{
    int pSize;      // ethernet packet size
    int nRead;      // bytes read from ethernet UDP port
    // 
    // local tracking of GPS/ref clock status
    //
    uint8_t hold;    
    uint8_t gotPps;
    uint8_t lock;
    
#if SYSCLK_DISCIPLINED  // Timers are not started in setup() when using disciplined clock
    if (!timersRunning)
    {
        //
        // wait until the GPS/ref clock hardware is operating normally before starting timers
        //
        if (TOD->AllConditionsNormal())
        {
            StartTimers(0U);          // defined in NtpHdwr.cpp
            timersRunning = 1;
            debug_str("! Timers started\r\n");
        }
    }
#else
    if (!timersRunning)
    {
        StartTimers(0U);
        timersRunning = 1U;
    }
#endif
    //
    // allow error LED to turn on only after warmup timeout has expired
    //
    if (!errorLedEnabled)
    {
        if (TIMEDOUT(millis(), errorTimeout)) errorLedEnabled = 1;
    }
    //
    // clarification on GPS states:
    // first of all, when the HP55300A is in holdover we consider this a valid locked condition
    // and do not consider the NTP server to be in holdover mode. This is due to the extremely good
    // performance of holdover mode in the HP55300A.
    //
    // When the ICM SMT 360 goes into holdover mode we consider this to be holdover -- it is only good
    // for maybe 10-15 minutes. Not sure if holdover can expire in this hdwr where it pulls the PPS signal.
    //
    // When the HP55300A pulls the PPS signal due to holdover time expired then the PLL goes into it's own
    // holdover mode and we consider that to be holdover...it only is good for maybe 10-15 minutes.
    //
    // there are several possible states:
    // 1. PLL locked and valid pps, no holdover
    // 2. PLL locked, no valid pps, holdover active ; this only occurs with non-disciplined clock.
    // 3. PLL unlocked -- holdover is of no concern here
    // 4. PLL locked, valid PPS, holdover active ; this only occurs with disciplined clock.
    //
    // the variables below indicate the following:
    // locked/pllLocked : true in (1), (2) and (4)
    // hold/holdover:     true in (2) and (4)
    //
#if SYSCLK_DISCIPLINED
    //
    // with the ICM SMT 360 GPS receiver, locked and holdover states reflect the 
    // state of the receiver itself.
    //
    gotPps = goodTimers.PpsOccurred();    
    lock = TOD->Locked() && TOD->PpsValid();
    hold = TOD->Holdover();

    ledHoldover.Set(hold);
    if (timeOfDayValid)
    {
        ledLock.Set(lock);
    }
    else
    {
        //
        // if GPS is locked but we don't have good UTC offset yet, then
        // flash the locked LED continuously until that status changes.
        //
        if (TOD->Locked())
        {
            ledLock.ConfigureFlash(500U, 1500U);
            ledLock.FlashContinuous();   // continuous flashing
        }
        else
        {
            ledLock.Set(0U);
        }
    }

#else
    //
    // with the HP55300A, the lock/holdover state reflects the PLL being locked to
    // the PPS from the refclock, NOT whether or not the refclock itself is locked or
    // in holdover mode.
    //
    gotPps = PLL.Step();
    lock = PLL.Locked();
    hold = PLL.Holdover();

    ledTod.Set(timeOfDayValid);
    ledLock.Set(lock);
#endif
        
    if (errorLedEnabled)
    {
        //
        // a holdover timeout will force pllLock false so we don't need to check for that separately
        //
        uint8_t err = (!pllLock && !holdover) || !timeOfDayValid;
        ledError.Set(err);
    }
    //
    // check for PLL lock changes and print a message if desired.
    //
    if (lock != pllLock)
    {
        debug_str(lock ? "! LOCKED\r\n" : "! UNLOCKED\r\n");
        pllLock = lock;
        ledLock.Set(pllLock);
    }
 
    if (hold != holdover)
    {
        debug_str(hold ? "! Holdover On\r\n" : "! Holdover Off\r\n");
        holdover = hold;
    }    
    //
    // update LEDs which might flash; static LEDs don't need this call (but it won't hurt anything)
    //
    ledLock.Update();
    ledPps.Update();
    ledRqst.Update();
    ledError.Update();
    //
    // did a PPS  timer capture occur?
    //
    if (gotPps)
    {
        // 
        // a new PPS interrupt has occurred; we don't do any significant processing 
        // here except to run the PLL if the CPU clock is not disciplined.
        //
        timerCycles = 0;
        if (timeCheck < 0x7fff) timeCheck++;  
        if (!hold) ledPps.Flash(1U);
        return;
    }
    //
    // No PPS interrupt, so normal processing is done only after a timer 4 overflow interrupt has occurred.
    //
    if (!timersRunning)
    {
        //
        // when timers not running, call this frequently to detect changes in GPS / ref clock state
        //
        checkTOD();
        //
        // check for an incoming NTP request packet (which we'll ignore)
        //
        if ((pSize = Udp.parsePacket()) > 0)
        {
            nRead = Udp.read(NtpPacket, NtpPacketSize);
            ledError.Flash(1U);
            Serial.print("! Skip["); Serial.print(nRead); Serial.println("]: init");
        }
    }
    else
    {
        //
        // 32 or 160 times per second, there will be timer match interrupts, counted as "unserviced interrupts".
        // we catch up by doing periodic operations until it is zeroed again.
        //
        while (keepTime->UnservicedInterruptCount()) {
            timerCycles++;
            ntpWindowTimeout = micros() + NTP_WINDOW_US;
            //
            // we check PPS tracking against time of day from GPS ref clock once in a while
            // this also might check to for holdover and other problems.
            // with the HP55300A, this takes 80ms to run so we break it up into separate tx and rx 
            // sections...assembling the response in subsequent passes. see getTimeOfDay() for details
            // For the Trimble ICM SMT 360, this runs faster but still requires a couple of timer cycles to 
            // complete.
            //
            // by requiring timeCheck >= 2 we know that PPS pulses are coming in and timerCycles
            // will accurately reflect how many cycles it has been since the last PPS signal.
            // This way we can avoid making the query too close to a PPS interrupt;  we want to complete the transaction
            // all within a single time span between two PPS pulses. Normally we will get the full
            // transaction done within 2-3 timer cycles but we allow 5 to be sure there's enough time
            //
            if ( (timeCheck >= 2) && (timerCycles >= TOD_CYCLE_MIN) && (timerCycles <= TOD_CYCLE_MAX) )
            {
                checkTOD();
            }
        }
        //
        // in cycle 0, a PPS has already occurred so we only have 1/2 cycle before next timer match interrupt
        // in cycles 1..(NOM_CYCLE_PER_SEC-2) we have 1 full cycle
        // in cycle (NOM_CYCLE_PER_SEC-1) we have 1/2 cycle before the PPS occurs
        //
        uint8_t ntpWindow = (timerCycles >= NTP_CYCLE_MIN) && (timerCycles <= NTP_CYCLE_MAX);

        if (ntpWindow && !TIMEDOUT(micros(), ntpWindowTimeout))
        {
            //
            // check for an incoming NTP request packet
            //
            if ((pSize = Udp.parsePacket()) > 0)
            {
                nRead = Udp.read(NtpPacket, NtpPacketSize); // this should clear the interrupt...?
                
                if (nRead < pSize)
                {
                    while (Udp.read() >= 0);    // dump any remaining data in the packet
                }
                //
                // make sure there is no I/O in process to serial ports because that will use
                // interrupts and might mess up our transmit timestamp accuracy.
                // I/O includes comms w/the GPS receiver and Serial.print functions to the USB port.
                //
                uint8_t ioBusy = TOD->IoInProcess();    // any I/O busy with GPS receiver?

                if (!ioBusy)
                {
                    //
                    // make sure we're not busy writing data to the USB serial port
                    // *** WARNING *** Some of this code is specific to the Arduino Mega and
                    // will not necessarily work on other Arduino boards. Specifically it depends
                    // on the fact that Serial uses USART0 in the Mega1280/2560 processor.
                    // one could if desired also check for incoming I/O but that is of questionable 
                    // value since we cannot predict future I/O from USB host.
                    //
                    if (Serial.availableForWrite() != (SERIAL_TX_BUFFER_SIZE - 1))
                    {
                        ioBusy = 1U;
                    }
                    else
                    {
                        //
                        // here we assume that Serial() uses USART0. This is true for Arduino Mega
                        // but perhaps not for other Arduino variants. If the tx data register
                        // empty bit is zero it means the UART is busy transmitting a byte and there
                        // might be an interrupt to follow.
                        //
                        if ((UCSR0A & _BV(UDRE0)) == 0U)
                        {
                            ioBusy = 1U;
                        }
                    }
                }                
                //
                // pllLock will go off when holdover time expires so we don't need to check that separately
                //
                uint8_t canReply = (pllLock || holdover) && (timeOfDayValid > 1U) && !ioBusy;
            
                if (canReply)
                {
                    if ((pSize >= NtpPacketSize) && (nRead == NtpPacketSize))
                    {
                        if (ntpServer->ProcessRequest(NtpPacket, NtpPacketSize))
                        {
                            ledRqst.Flash(1U);
                        }
                        else
                        {
                            ledError.Flash(1U);
                            debug_str("! NTP xmit fail\r\n");
                        }
                    }
                    else
                    {
                        ledError.Flash(1U);
                        debug_str("! NTP rqst size\r\n");
                    }
                }
                else
                {
                    ledError.Flash(1U);
                    Serial.print("! Skip: ");
                    if (!pllLock) Serial.print("L");
                    if (!holdover) Serial.print("H");
                    if (timeOfDayValid < 2U) Serial.print("T");
                    if (ioBusy) Serial.print("I");
                    Serial.println();
                }

                // ntpServer->UpdateAssociations();
                //
                // flush any packets that have arrive during the procesing of the current request
                // because we won't have an accurate timestamp on them
                //
                uint8_t flushed = 0;
                while (Udp.parsePacket() > 0)
                {
                    do
                    {
                        nRead = Udp.read(NtpPacket, NtpPacketSize);
                    } while (nRead > 0);
                    flushed++;
                }
                if (flushed) 
                {
                    Serial.print("! F-0x"); NtpUtils::printHex(flushed, 1U);
                }
                //
                // the ProcessRequest method may clear the capture flag, but we do it again here
                // just in case we skipped the reply and to be doubly sure it gets cleared
                //
                RESET_UDP_CAPTURE_FLAG;
            }
            else
            {
                // RESET_UDP_CAPTURE_FLAG;
            }
        }
    }
}
