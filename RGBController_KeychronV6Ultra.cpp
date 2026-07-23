/*---------------------------------------------------------*\
| RGBController_KeychronV6Ultra.cpp                         |
\*---------------------------------------------------------*/

#include "RGBController_KeychronV6Ultra.h"

/*---------------------------------------------------------*\
| Modes                                                      |
\*---------------------------------------------------------*/
enum
{
    MODE_DIRECT = 0,
};

/*---------------------------------------------------------*\
| Physical key layout — generated from the firmware's        |
| g_led_config.point[] coordinates (6 rows x 22 cols).       |
| Cell = LED index (same index we drive), NO_LED = gap.      |
\*---------------------------------------------------------*/
#ifndef NO_LED
#define NO_LED 0xFFFFFFFF   /* OpenRGB matrix-map gap marker */
#endif

#define V6U_MAP_H 6
#define V6U_MAP_W 22

static unsigned int v6u_matrix_map[V6U_MAP_H * V6U_MAP_W] =
{
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, NO_LED, NO_LED, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, NO_LED,     33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, NO_LED,     54, 55, 56, 57, 58, 59, 60, 77,
    61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, NO_LED, NO_LED, 73, NO_LED, NO_LED, NO_LED, 74, 75, 76, NO_LED,
    78, NO_LED, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, NO_LED, NO_LED, 89, NO_LED, 90, NO_LED, 91, 92, 93, 107,
    94, 95, 96, NO_LED, NO_LED, NO_LED, 97, NO_LED, NO_LED, NO_LED, 98, 99, 100, NO_LED, 101, 102, 103, 104, NO_LED, 105, 106, NO_LED,
};

static matrix_map_type v6u_matrix = { V6U_MAP_H, V6U_MAP_W, v6u_matrix_map };

/*---------------------------------------------------------*\
| Per-LED names (LED index order). These MUST match the      |
| strings RGB.NET's OpenRGB provider maps to LedIds          |
| (RGB.NET.Devices.OpenRGB/Generic/LedMappings.cs), so       |
| Artemis assigns real keys instead of colliding on a        |
| fallback. Positions come from the matrix map above.        |
\*---------------------------------------------------------*/
static const char* v6u_led_names[KEYCHRON_V6U_LED_COUNT] =
{
    /* Derived authoritatively from the firmware g_led_config matrix_co +      */
    /* default_transform map + Windows keymap layer (see rgb_matrix_config.h    */
    /* and keychron_v6_ultra_ansi.{overlay,keymap}). Every string is a verified */
    /* key in RGB.NET LedMappings.DEFAULT so Artemis assigns a real LedId.      */
    /* LEDs 13-19 are the top-right custom keys (PrtSc + 6 Keychron macro keys) */
    /* mapped to otherwise-unused standard LedIds purely so they light.         */
    /* 0-12  Esc + F1-F12                                                       */
    "Key: Escape", "Key: F1", "Key: F2", "Key: F3", "Key: F4", "Key: F5",
    "Key: F6", "Key: F7", "Key: F8", "Key: F9", "Key: F10", "Key: F11", "Key: F12",
    /* 13-19 top-right custom cluster (PrtSc, Snip, RGB, F13-F16)               */
    "Key: Print Screen", "Key: Scroll Lock", "Key: Pause/Break",
    "Key: Media Mute", "Key: Media Play/Pause", "Key: Media Previous", "Key: Media Next",
    /* 20-40 number row + nav-top + numpad-top                                  */
    "Key: `", "Key: 1", "Key: 2", "Key: 3", "Key: 4", "Key: 5", "Key: 6", "Key: 7",
    "Key: 8", "Key: 9", "Key: 0", "Key: -", "Key: =", "Key: Backspace",
    "Key: Insert", "Key: Home", "Key: Page Up",
    "Key: Num Lock", "Key: Number Pad /", "Key: Number Pad *", "Key: Number Pad -",
    /* 41-60 QWERTY row + nav-mid + numpad-7/8/9                                */
    "Key: Tab", "Key: Q", "Key: W", "Key: E", "Key: R", "Key: T", "Key: Y", "Key: U",
    "Key: I", "Key: O", "Key: P", "Key: [", "Key: ]", "Key: \\ (ANSI)",
    "Key: Delete", "Key: End", "Key: Page Down",
    "Key: Number Pad 7", "Key: Number Pad 8", "Key: Number Pad 9",
    /* 61-77 home row + numpad-4/5/6 + numpad-plus                              */
    "Key: Caps Lock", "Key: A", "Key: S", "Key: D", "Key: F", "Key: G", "Key: H",
    "Key: J", "Key: K", "Key: L", "Key: ;", "Key: '", "Key: Enter",
    "Key: Number Pad 4", "Key: Number Pad 5", "Key: Number Pad 6", "Key: Number Pad +",
    /* 78-93 shift row + up-arrow + numpad-1/2/3                                */
    "Key: Left Shift", "Key: Z", "Key: X", "Key: C", "Key: V", "Key: B", "Key: N",
    "Key: M", "Key: ,", "Key: .", "Key: /", "Key: Right Shift",
    "Key: Up Arrow",
    "Key: Number Pad 1", "Key: Number Pad 2", "Key: Number Pad 3",
    /* 94-107 bottom row + arrows + numpad-0/enter/dot                          */
    "Key: Left Control", "Key: Left Windows", "Key: Left Alt", "Key: Space",
    "Key: Right Alt", "Key: Right Windows", "Key: Menu", "Key: Right Control",
    "Key: Left Arrow", "Key: Down Arrow", "Key: Right Arrow",
    "Key: Number Pad 0", "Key: Number Pad Enter", "Key: Number Pad .",
};

RGBController_KeychronV6Ultra::RGBController_KeychronV6Ultra(KeychronV6UltraController* controller_ptr)
{
    controller  = controller_ptr;

    name        = "Keychron V6 Ultra 8K";
    vendor      = "Keychron";
    type        = DEVICE_TYPE_KEYBOARD;
    description = "Keychron V6/Q6 Ultra (custom ZMK firmware, OpenRGB direct control)";
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
    delete controller;
}

void RGBController_KeychronV6Ultra::SetupZones()
{
    zone kb_zone;
    kb_zone.name       = "Keyboard";
    kb_zone.type       = ZONE_TYPE_MATRIX;
    kb_zone.leds_min   = KEYCHRON_V6U_LED_COUNT;
    kb_zone.leds_max   = KEYCHRON_V6U_LED_COUNT;
    kb_zone.leds_count = KEYCHRON_V6U_LED_COUNT;
    kb_zone.matrix_map = &v6u_matrix;
    zones.push_back(kb_zone);

    for(unsigned int i = 0; i < KEYCHRON_V6U_LED_COUNT; i++)
    {
        led new_led;
        new_led.name  = v6u_led_names[i];
        new_led.value = i;
        leds.push_back(new_led);
    }

    SetupColors();
}

void RGBController_KeychronV6Ultra::ResizeZone(int /*zone*/, int /*new_size*/)
{
    /* fixed 108-LED layout — nothing to resize */
}

void RGBController_KeychronV6Ultra::DeviceUpdateLEDs()
{
    controller->EnsureDirect();   // start direct mode + keepalive on first update
    controller->SetLEDs(colors);
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
    /* Direct is the only host-driven mode; entering it takes the keyboard over  |
     | (and starts the keepalive), leaving it hands back to onboard lighting.    */
    controller->SetDirectMode(active_mode == MODE_DIRECT);

    if(active_mode == MODE_DIRECT)
    {
        DeviceUpdateLEDs();
    }
}

void RGBController_KeychronV6Ultra::SetCustomMode()
{
    active_mode = MODE_DIRECT;
}
