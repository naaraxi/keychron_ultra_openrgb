/*---------------------------------------------------------*\
| RGBController_KeychronV6Ultra.cpp                         |
|                                                           |
|   Generic OpenRGB wrapper for any Keychron Ultra board    |
|   running the custom ZMK firmware. All board-specific     |
|   data (LED count, physical matrix map, per-key names)    |
|   comes from the KeychronLayout descriptor selected by    |
|   USB PID in the plugin — see KeychronLayouts.h.          |
\*---------------------------------------------------------*/

#include "RGBController_KeychronV6Ultra.h"

/*---------------------------------------------------------*\
| Modes                                                      |
\*---------------------------------------------------------*/
enum
{
    MODE_DIRECT = 0,
};

RGBController_KeychronV6Ultra::RGBController_KeychronV6Ultra(KeychronV6UltraController* controller_ptr,
                                                            const KeychronLayout* layout_ptr)
{
    controller  = controller_ptr;
    layout      = layout_ptr;

    name        = layout->name;
    vendor      = "Keychron";
    type        = DEVICE_TYPE_KEYBOARD;
    description = layout->description;
    location    = controller->GetLocation();
    serial      = controller->GetSerialString();

    mode Direct;
    Direct.name       = "Direct";
    Direct.value      = MODE_DIRECT;
    Direct.flags      = MODE_FLAG_HAS_PER_LED_COLOR;
    Direct.color_mode = MODE_COLORS_PER_LED;
    modes.push_back(Direct);

    SetupZones();

    active_mode = MODE_DIRECT;
}

RGBController_KeychronV6Ultra::~RGBController_KeychronV6Ultra()
{
    /*-----------------------------------------------------------------------*\
    | RGBController's constructor starts DeviceCallThread, which calls the    |
    | virtual DeviceUpdateLEDs()/DeviceUpdateMode() in a loop, and only the   |
    | BASE destructor stops it - base destructors run after this body. Freeing |
    | the hardware controller here unguarded is therefore a use-after-free    |
    | whenever an update is in flight, and that is a routine path rather than |
    | a shutdown-only one: OpenRGB's Cleanup() destroys plugin-registered     |
    | controllers on every "Rescan Devices".                                  |
    |                                                                        |
    | Taking the same mutex the update methods take means a concurrent update |
    | either completes before the pointer goes away or sees nullptr and does  |
    | nothing.                                                                |
    |                                                                        |
    | This also closes the matching race in ~KeychronV6UltraController, which |
    | stops and joins the keepalive thread without holding its own            |
    | state_mutex: DeviceUpdateMode() is the only caller of SetDirectMode()   |
    | and can no longer run while this lock is held.                          |
    |                                                                        |
    | Residual, not fixable from a plugin: once ~RGBController begins, the    |
    | vptr is demoted while its thread is still running, so a call flag set   |
    | in that window dispatches a pure virtual. CallFlag_*, DeviceCallThread  |
    | and DeviceThreadRunning are all private in RGBController, so a subclass |
    | cannot drain or stop the thread earlier.                                |
    \*-----------------------------------------------------------------------*/
    std::lock_guard<std::mutex> lock(controller_mutex);

    delete controller;
    controller = nullptr;
}

void RGBController_KeychronV6Ultra::SetupZones()
{
    /*-----------------------------------------------------------------------*\
    | matrix backs the zone's matrix_map pointer, so it must outlive setup —  |
    | it is a member. map/height/width come straight from the layout.         |
    \*-----------------------------------------------------------------------*/
    matrix.height = layout->map_height;
    matrix.width  = layout->map_width;
    matrix.map    = layout->matrix_map;

    zone kb_zone;
    kb_zone.name       = "Keyboard";
    kb_zone.type       = ZONE_TYPE_MATRIX;
    kb_zone.leds_min   = layout->led_count;
    kb_zone.leds_max   = layout->led_count;
    kb_zone.leds_count = layout->led_count;
    kb_zone.matrix_map = &matrix;
    zones.push_back(kb_zone);

    for(unsigned int i = 0; i < layout->led_count; i++)
    {
        led new_led;
        new_led.name  = layout->led_names[i];
        new_led.value = i;
        leds.push_back(new_led);
    }

    SetupColors();
}

void RGBController_KeychronV6Ultra::ResizeZone(int /*zone*/, int /*new_size*/)
{
    /* fixed per-board layout — nothing to resize */
}

void RGBController_KeychronV6Ultra::DeviceUpdateLEDs()
{
    std::lock_guard<std::mutex> lock(controller_mutex);

    UpdateLEDsLocked();
}

void RGBController_KeychronV6Ultra::UpdateLEDsLocked()
{
    if(controller == nullptr)
    {
        return;                   /* torn down by a rescan - nothing to drive */
    }

    controller->EnsureDirect();   // start direct mode + keepalive on first update
    controller->SetLEDs(colors);

    /*-----------------------------------------------------------------------*\
    | If a reconnect moved the keyboard to a different HID path, refresh the  |
    | location we cached at construction. Otherwise OpenRGB (and the SDK, and |
    | anything reading it such as Artemis) keeps reporting the old node long  |
    | after it stopped existing, which makes this failure very hard to        |
    | diagnose. Cheap: an atomic exchange on the common no-change path.       |
    \*-----------------------------------------------------------------------*/
    if(controller->TakeLocationChanged())
    {
        location = controller->GetLocation();
    }
}

void RGBController_KeychronV6Ultra::UpdateZoneLEDs(int /*zone*/)
{
    DeviceUpdateLEDs();
}

void RGBController_KeychronV6Ultra::UpdateSingleLED(int /*led*/)
{
    DeviceUpdateLEDs();
}

void RGBController_KeychronV6Ultra::DeviceUpdateMode()
{
    std::lock_guard<std::mutex> lock(controller_mutex);

    if(controller == nullptr)
    {
        return;
    }

    /* Direct is the only host-driven mode; entering it takes the keyboard over  |
     | (and starts the keepalive), leaving it hands back to onboard lighting.    */
    controller->SetDirectMode(active_mode == MODE_DIRECT);

    if(active_mode == MODE_DIRECT)
    {
        UpdateLEDsLocked();       /* already holding the lock - must not re-take it */
    }
}

void RGBController_KeychronV6Ultra::SetCustomMode()
{
    active_mode = MODE_DIRECT;
}
