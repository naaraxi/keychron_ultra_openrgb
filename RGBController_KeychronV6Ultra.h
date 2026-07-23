/*---------------------------------------------------------*\
| RGBController_KeychronV6Ultra.h                           |
|                                                           |
|   OpenRGB RGBController wrapper for the Keychron V6 Ultra  |
|   8K custom-firmware OpenRGB direct-control support.       |
\*---------------------------------------------------------*/

#pragma once

#include "RGBController.h"
#include "KeychronV6UltraController.h"

class RGBController_KeychronV6Ultra : public RGBController
{
public:
    RGBController_KeychronV6Ultra(KeychronV6UltraController* controller_ptr);
    ~RGBController_KeychronV6Ultra();

    void        SetupZones();
    void        ResizeZone(int zone, int new_size);

    void        DeviceUpdateLEDs();
    void        UpdateZoneLEDs(int zone);
    void        UpdateSingleLED(int led);

    void        DeviceUpdateMode();
    void        SetCustomMode();

private:
    KeychronV6UltraController* controller;
};
