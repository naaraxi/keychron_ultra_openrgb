/*---------------------------------------------------------*\
| KeychronV6UltraController.cpp                             |
|   Talks the custom-firmware OpenRGB command (0x16) over   |
|   the raw-HID channel (usage 0xFF60), 32-byte reports,    |
|   unnumbered report id (0x00 prefix on write).            |
\*---------------------------------------------------------*/

#include "KeychronV6UltraController.h"
#include <chrono>
#include <cstring>

/* id_openrgb (0x16) subcommands — must match app/src/launcher/openrgb.c */
#define OPENRGB_CMD              0x16
#define SUB_GET_PROTOCOL_VERSION 0x00
#define SUB_GET_LED_COUNT        0x01
#define SUB_SET_DIRECT_MODE      0x02
#define SUB_SET_LEDS             0x03
#define SUB_GET_LED_INFO         0x04

#define KEEPALIVE_INTERVAL_MS    1500

KeychronV6UltraController::KeychronV6UltraController(hid_device* dev_handle, const char* path)
{
    dev           = dev_handle;
    location      = path;
    direct_active = false;
    keepalive_run = false;
}

KeychronV6UltraController::~KeychronV6UltraController()
{
    keepalive_run = false;
    if(keepalive_thread.joinable())
    {
        keepalive_thread.join();
    }
    if(dev)
    {
        /* Best-effort: return control to onboard lighting on unload. */
        unsigned char pkt[3] = { OPENRGB_CMD, SUB_SET_DIRECT_MODE, 0 };
        xfer(pkt, sizeof(pkt), nullptr);
        hid_close(dev);
    }
}

std::string KeychronV6UltraController::GetLocation()
{
    return("HID: " + location);
}

std::string KeychronV6UltraController::GetSerialString()
{
    wchar_t serial[128];
    if(hid_get_serial_number_string(dev, serial, 128) != 0)
    {
        return("");
    }
    std::wstring w(serial);
    return(std::string(w.begin(), w.end()));
}

/*---------------------------------------------------------*\
| Send one command; optionally read the echoed response.    |
| Returns bytes read into resp, or -1. Mutex-guarded.       |
\*---------------------------------------------------------*/
int KeychronV6UltraController::xfer(const unsigned char* payload, size_t len, unsigned char* resp)
{
    std::lock_guard<std::mutex> lock(hid_mutex);

    unsigned char buf[KEYCHRON_V6U_EPSIZE + 1];
    memset(buf, 0x00, sizeof(buf));
    buf[0] = 0x00;                                   /* report id 0 (unnumbered) */
    memcpy(&buf[1], payload, len > KEYCHRON_V6U_EPSIZE ? KEYCHRON_V6U_EPSIZE : len);

    if(hid_write(dev, buf, KEYCHRON_V6U_EPSIZE + 1) < 0)
    {
        return(-1);
    }

    if(resp != nullptr)
    {
        return(hid_read_timeout(dev, resp, KEYCHRON_V6U_EPSIZE, 500));
    }
    return(0);
}

bool KeychronV6UltraController::IsOpenRGBFirmware()
{
    return(GetLEDCount() == KEYCHRON_V6U_LED_COUNT);
}

unsigned int KeychronV6UltraController::GetLEDCount()
{
    unsigned char pkt[2] = { OPENRGB_CMD, SUB_GET_LED_COUNT };
    unsigned char resp[KEYCHRON_V6U_EPSIZE] = { 0 };
    if(xfer(pkt, sizeof(pkt), resp) > 0 && resp[0] == OPENRGB_CMD)
    {
        return((unsigned int)(resp[2] | (resp[3] << 8)));
    }
    return(0);
}

void KeychronV6UltraController::SetDirectMode(bool enable)
{
    /*-----------------------------------------------------------------------*\
    | Always stop AND join any running keepalive before changing state, so we |
    | never reassign a still-joinable std::thread (that calls std::terminate  |
    | and crashes the host). SetDirectMode is only called on mode changes, so |
    | the join cost is irrelevant.                                            |
    \*-----------------------------------------------------------------------*/
    std::lock_guard<std::mutex> state_lock(state_mutex);

    if(keepalive_run.exchange(false) && keepalive_thread.joinable())
    {
        keepalive_thread.join();
    }

    unsigned char pkt[3] = { OPENRGB_CMD, SUB_SET_DIRECT_MODE, (unsigned char)(enable ? 1 : 0) };
    xfer(pkt, sizeof(pkt), nullptr);
    direct_active = enable;

    if(enable)
    {
        keepalive_run    = true;
        keepalive_thread = std::thread(&KeychronV6UltraController::KeepaliveLoop, this);
    }
}

void KeychronV6UltraController::EnsureDirect()
{
    if(!direct_active)
    {
        SetDirectMode(true);
    }
}

/*---------------------------------------------------------*\
| Re-arm the firmware's 3s auto-hand-back watchdog while     |
| OpenRGB holds the device, so colors persist between edits. |
\*---------------------------------------------------------*/
void KeychronV6UltraController::KeepaliveLoop()
{
    while(keepalive_run)
    {
        for(int i = 0; i < KEEPALIVE_INTERVAL_MS / 50 && keepalive_run; i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if(keepalive_run)
        {
            unsigned char pkt[3] = { OPENRGB_CMD, SUB_SET_DIRECT_MODE, 1 };
            xfer(pkt, sizeof(pkt), nullptr);
        }
    }
}

/*---------------------------------------------------------*\
| Stream all LED colors in runs of <=9 (fits one 32-byte    |
| packet: 4 header bytes + 9*3 RGB).                        |
\*---------------------------------------------------------*/
void KeychronV6UltraController::SetLEDs(const std::vector<RGBColor>& colors)
{
    unsigned int total = (unsigned int)colors.size();
    unsigned int i     = 0;

    while(i < total)
    {
        unsigned int cnt = (total - i) < KEYCHRON_V6U_LEDS_PER_PKT
                         ? (total - i) : KEYCHRON_V6U_LEDS_PER_PKT;

        unsigned char pkt[4 + KEYCHRON_V6U_LEDS_PER_PKT * 3];
        pkt[0] = OPENRGB_CMD;
        pkt[1] = SUB_SET_LEDS;
        pkt[2] = (unsigned char)i;                   /* start index */
        pkt[3] = (unsigned char)cnt;                 /* run length  */

        for(unsigned int j = 0; j < cnt; j++)
        {
            RGBColor c         = colors[i + j];
            pkt[4 + j * 3 + 0] = RGBGetRValue(c);
            pkt[4 + j * 3 + 1] = RGBGetGValue(c);
            pkt[4 + j * 3 + 2] = RGBGetBValue(c);
        }

        xfer(pkt, 4 + cnt * 3, nullptr);
        i += cnt;
    }
}

bool KeychronV6UltraController::GetLEDPosition(unsigned int idx, unsigned int& x,
                                               unsigned int& y, unsigned int& flags)
{
    unsigned char pkt[3]  = { OPENRGB_CMD, SUB_GET_LED_INFO, (unsigned char)idx };
    unsigned char resp[KEYCHRON_V6U_EPSIZE] = { 0 };
    if(xfer(pkt, sizeof(pkt), resp) > 0 && resp[0] == OPENRGB_CMD)
    {
        x     = resp[3];   /* args[1] = x (resp[0]=cmd,[1]=sub,[2]=idx echo,[3]=x) */
        y     = resp[4];   /* args[2] = y */
        flags = resp[5];   /* args[3] = flags */
        return(true);
    }
    return(false);
}
