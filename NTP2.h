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

#ifndef NTP2_H
#define NTP2_H

#include "Arduino.h"
#include <time.h>
#include <Udp.h>

#define SEVENTYYEARS 2208988800UL
#define NTP_PACKET_SIZE 48
#define NTP_PORT 123
#define NTP_WAIT 1000 // delay before unpacking packet, in ms
#define NTP_EINTERVAL 60000;  // 1 minute retry delay after bad read
#define NTP_WINTERVAL 1800000; // 30 minutes between successful tries

class NTP2 {
  public:
    /**
     * @brief constructor for the NTP object
     * 
     * @param udp 
     */
    NTP2(UDP& udp);
	
    /**
     * @brief destructor for the NTP object
     * 
     */
    ~NTP2();

    /**
     * @brief starts the underlying UDP client with the default local port
     * 
     * @param server NTP server host name
     * @param serverIP NTP server IP address
     */
    void begin(const char* server = "us.pool.ntp.org");
    void begin(IPAddress serverIP);

    /**
     * @brief stops the underlying UDP client
     * 
     */
    void stop();

	/**
     * @additional packet validation taken in desparation from ezTime.cpp
     * 
     */
    bool checkValid();
	
	/**
     * @initialize NTP 
     * 
     */
    void init();
	
	/**
     * @brief This should be called in the main loop of your application. By default an update from the NTP Server is only
     * made every 30 minutes. This can be configured in the NTPTime constructor.
     * 
     * @return true on success
     * @return false on no update or update failure
     */
    byte update();
	
	/**
     * @brief force immediate update
     * 
     * @param return true on success
     */
	 byte forceUpdate();

    /**
     * @brief set the update interval
     * 
     * @param updateInterval in ms, default = 1,800,0000ms
     */
	 void updateInterval(uint32_t interval);

	 /**
	 @brief returns status of last NTP update attempt
	 */
	 bool ntpStat();
	
    /**
     * @brief get the Unix epoch timestamp
     * 
     * @return time_t timestamp
     */
    time_t epoch();

  private:
    UDP *udp;
    const char* server = nullptr;
    IPAddress serverIP;
    uint8_t ntpRequest[NTP_PACKET_SIZE] = {0xE3, 0x00, 0x06, 0xEC};
    uint8_t ntpQuery[NTP_PACKET_SIZE];
    time_t utcCurrent = 0;
    struct tm *current;
    uint32_t interval = 0; // current interval setting
	uint32_t oInterval;
    uint32_t lastUpdate = 0;
    uint32_t utcTime = 0;
	uint32_t lastUtcTime = 0;
    int32_t diffTime;    
	uint32_t waiting = 0;
	bool ntpstat = true; 
	bool force = false;
	
	// Define the static lookup table with initialization
	struct KodEntry {
        const char *code;
        byte ret;
    };
    // C++17 inline static member definition in the header
    inline static const KodEntry kodLookup[] = {
        {"RATE", 0x2},
        {"DENY", 0x3},
        {"ACST", 0x4},
        {"AUTH", 0x5},
        {"AUTO", 0x6},
        {"BCST", 0x7},
        {"CRYP", 0x8},
        {"DROP", 0x9},
        {"RSTR", 0xA},
        {"INIT", 0xB},
        {"MCST", 0xC},
        {"NKEY", 0xD},
        {"NTSN", 0xE},
        {"RMOT", 0xF},
        {"STEP", 0x10}
    };
};

#endif
