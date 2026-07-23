/*---------------------------------------------------------*\
| KeychronV6UltraController.h                               |
|                                                           |
|   Driver for the Keychron V6 Ultra 8K running custom ZMK  |
|   firmware with OpenRGB direct-control support (issue     |
|   #893). Speaks the id_openrgb (0x16) command over the    |
|   raw-HID channel (usage page 0xFF60), 32-byte reports.   |
|                                                           |
|   Wire format:  [0x00 reportid][0x16][sub][args...]       |
|     sub 0x00 GET_PROTOCOL_VERSION -> args[0]              |
|     sub 0x01 GET_LED_COUNT        -> args[0..1] LE        |
|     sub 0x02 SET_DIRECT_MODE      args[0]=1 on/0 off      |
|     sub 0x03 SET_LEDS  args[0]=start,args[1]=count,       |
|                        then count*3 RGB bytes (<=9/pkt)   |
|     sub 0x04 GET_LED_INFO args[0]=idx -> args[1..3]=x,y,fl|
\*---------------------------------------------------------*/

#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <hidapi.h>
#include "RGBController.h"

#define KEYCHRON_V6U_LED_COUNT    108
#define KEYCHRON_V6U_EPSIZE       32
#define KEYCHRON_V6U_LEDS_PER_PKT 9

class KeychronV6UltraController
{
public:
    KeychronV6UltraController(hid_device* dev_handle, const char* path);
    ~KeychronV6UltraController();

    std::string    GetLocation();
    std::string    GetSerialString();
    unsigned int   GetLEDCount();          // queries the device (expects 108)
    bool           IsOpenRGBFirmware();    // false if device answers 0xFF (stock fw)

    void           SetDirectMode(bool enable);
    void           EnsureDirect();         // enter direct + start keepalive if not already
    void           SetLEDs(const std::vector<RGBColor>& colors);
    bool           GetLEDPosition(unsigned int idx, unsigned int& x, unsigned int& y,
                                  unsigned int& flags);

private:
    hid_device*        dev;
    std::string        location;
    std::mutex         hid_mutex;         // serialize HID I/O (OpenRGB thread + keepalive)
    std::mutex         state_mutex;       // serialize direct-mode / keepalive lifecycle

    std::atomic<bool>  direct_active;
    std::atomic<bool>  keepalive_run;
    std::thread        keepalive_thread;  // re-arms firmware watchdog while direct-mode is on

    int  xfer(const unsigned char* payload, size_t len, unsigned char* resp);
    void KeepaliveLoop();
};
