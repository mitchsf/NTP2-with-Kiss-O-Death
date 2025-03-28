/**
   NTP2 library for Arduino framework
  THIS IS NTP1 WITH DST AND TIME FUNCTIONS REMOVED *********** ALL I WANT IS EPOCH TIME!
   The MIT License (MIT)
   (c) 2022 sstaub

   Modified by Mitch to split the NTP get and unpack without blocking 8/19/22
   More udates to attempt to fix bad read that happens a couple times each year so impossible to find. Search 04/13/23
   Updated with bad NTP packet filtering code from ezTime on 04/16/23
   Turns out it was Kiss O Death messages coming from NTP server
   Added forceUpdate() on 3/28/25

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

#include "NTP2.h"

NTP2::NTP2(UDP& udp) {
  this->udp = &udp;
}

NTP2::~NTP2() {
  stop();
}

void NTP2::begin(const char* server) {
  this->server = server;
  udp->begin(NTP_PORT);
  update(); // force get
  while (!ntpStat()) update(); // wait for connection
  update(); // unpack
}

void NTP2::begin(IPAddress serverIP) {
  this->serverIP = serverIP;
  udp->begin(NTP_PORT);
  update(); // force get
  while (!ntpStat()) update(); // wait for connection
  update(); // unpack
}

void NTP2::stop() {
  udp->stop();
}

byte NTP2::forceUpdate() {
force = true;
return update();
}

byte NTP2::update() {
  if (((millis() - lastUpdate >= interval) || lastUpdate == 0 || force) && waiting == 0) {
    udp->flush(); // probably not needed
    init();
    if (server == nullptr) udp->beginPacket(serverIP, NTP_PORT);
    else udp->beginPacket(server, NTP_PORT);
    udp->write(ntpRequest, NTP_PACKET_SIZE);
    udp->endPacket();
    waiting = millis(); // get packet, waiting to unpack
	force = false;
    return 0b0; // just getting packet so always return false
  }
  
  if (millis() - waiting >= NTP_WAIT) { // see if it's there after NTP_WAIT ms and unpack
    waiting = 0; // one try only - if it's not there after NTP_WAIT ms, it won't be there at all this poll
    uint8_t size = udp->parsePacket();
    if (size == NTP_PACKET_SIZE) { // 48 bytes
      lastUpdate = millis() - waiting;
      udp->read(ntpQuery, NTP_PACKET_SIZE);

      // Check for Kiss-o'-Death packet
    if (ntpQuery[1] == 0) { // Stratum 0 indicates KoD
    char kodMessage[5] = {ntpQuery[12], ntpQuery[13], ntpQuery[14], ntpQuery[15], '\0'};
    ntpstat = false;

    // Iterate through the table to find a matching code
	 for (size_t i = 0; i < sizeof(kodLookup) / sizeof(kodLookup[0]); i++) {
        if (strcmp(kodMessage, kodLookup[i].code) == 0)
            return kodLookup[i].ret;
    }
}

      uint32_t ntpTime = ntpQuery[40] << 24 | ntpQuery[41] << 16 | ntpQuery[42] << 8 | ntpQuery[43];
      utcTime = ntpTime - SEVENTYYEARS + NTP_WAIT_SECONDS; // wait NTP_WAIT so add NTP_WAIT
      interval = oInterval; // good read so set to original interval
      if (lastUtcTime == 0) lastUtcTime = utcTime;
      if (utcTime >= lastUtcTime  && checkValid()) { // more updates to fix bad read 04/13/23
        // check if new time is greater than last time. Also check that new time is not more than one day past last time if last time is not 0.
        // really just shots in the dark to figure out why the time is bad once or twice each year.
        // ntpQuery[1] = Stratum
        lastUtcTime = utcTime;
        ntpstat = true; // good ntp
        return 0b01;
      }
      else {
        interval = NTP_EINTERVAL; // bad read so shorten interval
        ntpstat = false; // bad ntp read
        utcTime = 0;
        lastUtcTime = 0;
        return 0b0;
      }

    }
  }
  else {
    interval = NTP_EINTERVAL; // bad read so shorten interval
    ntpstat = false; // bad ntp read
    utcTime = 0;
    lastUtcTime = 0;
  }
  return 0b0;
}

void NTP2::init() {
  memset(ntpRequest, 0, NTP_PACKET_SIZE);
  ntpRequest[0] = 0b11100011; // LI, Version, Mode
  ntpRequest[1] = 0;          // Stratum, or type of clock
  ntpRequest[2] = 6;          // Polling Interval
  ntpRequest[3] = 0xEC;       // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  ntpRequest[12]  = 49;
  ntpRequest[13]  = 0x4E;
  ntpRequest[14]  = 49;
  ntpRequest[15]  = 52;
}


bool NTP2::checkValid() { // some of this code was taken from ezTime and some is redundant.
  // Also it will ignore messages sent when Stratum (ntpQuery[1]) is equal to zero.
  //prepare timestamps
  uint32_t highWord, lowWord;
  highWord = (ntpQuery[16] << 8 | ntpQuery[17] ) & 0x0000FFFF;
  lowWord = (ntpQuery[18] << 8 | ntpQuery[19] ) & 0x0000FFFF;
  uint32_t reftsSec = highWord << 16 | lowWord;       // reference timestamp seconds
  highWord = (ntpQuery[32] << 8 | ntpQuery[33] ) & 0x0000FFFF;
  lowWord = (ntpQuery[34] << 8 | ntpQuery[35] ) & 0x0000FFFF;
  uint32_t rcvtsSec = highWord << 16 | lowWord;       // receive timestamp seconds
  highWord = (ntpQuery[40] << 8 | ntpQuery[41] ) & 0x0000FFFF;
  lowWord = (ntpQuery[42] << 8 | ntpQuery[43] ) & 0x0000FFFF;
  uint32_t secsSince1900 = highWord << 16 | lowWord;      // transmit timestamp seconds

  highWord = (ntpQuery[44] << 8 | ntpQuery[45] ) & 0x0000FFFF;
  lowWord = (ntpQuery[46] << 8 | ntpQuery[47] ) & 0x0000FFFF;
  uint32_t fraction = highWord << 16 | lowWord;       // transmit timestamp fractions
  //check if received data makes sense
  //buffer[1] = stratum - should be 1..15 for valid reply
  //also checking that all timestamps are non-zero and receive timestamp seconds are <= transmit timestamp seconds
  if ((ntpQuery[1] < 1) or (ntpQuery[1] > 15) or (reftsSec == 0) or (rcvtsSec == 0) or (rcvtsSec > secsSince1900)) {
    // we got invalid packet
    return false;
  }
  else return true;
}
bool NTP2::ntpStat() {
  return this->ntpstat;
}

void NTP2::updateInterval(uint32_t interval) {
  this->interval = interval;
  this->oInterval = interval;
}

time_t NTP2::epoch() {
  return utcTime + ((millis() - lastUpdate) / 1000);
}
