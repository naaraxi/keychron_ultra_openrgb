/*---------------------------------------------------------*\
| OpenRGBKeychronV6UltraPlugin.cpp                          |
\*---------------------------------------------------------*/

#include "OpenRGBKeychronV6UltraPlugin.h"
#include "KeychronV6UltraController.h"
#include <hidapi.h>
#include <QLabel>

#define KEYCHRON_VID        0x3434
#define KEYCHRON_V6U_PID    0x0C60
#define KEYCHRON_Q6U_PID    0x1260
#define RAW_USAGE_PAGE      0xFF60
#define RAW_USAGE           0x61

/*---------------------------------------------------------------------------*\
| Supported boards running the custom ZMK firmware. Same protocol / LED count; |
| only the USB PID and the display name differ (V6 = 0x0C60, Q6 = 0x1260).     |
\*---------------------------------------------------------------------------*/
static const struct
{
    unsigned short  pid;
    const char*     name;
} KEYCHRON_DEVICES[] =
{
    { KEYCHRON_V6U_PID, "Keychron V6 Ultra 8K" },
    { KEYCHRON_Q6U_PID, "Keychron Q6 Ultra 8K" },
};

OpenRGBPluginInfo OpenRGBKeychronV6UltraPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;
    info.Name          = "Keychron V6 Ultra (OpenRGB direct)";
    info.Description    = "Direct per-key RGB control for the Keychron V6 Ultra 8K "
                          "running custom ZMK firmware (issue #893).";
    info.Version        = "0.2.0";
    info.Commit         = "";
    info.URL            = "https://github.com/naaraxi/keychron_v6u_openrgb";
    info.Location       = OPENRGB_PLUGIN_LOCATION_SETTINGS;
    info.Label          = "Keychron V6U";
    info.TabIconString  = "";
    return(info);
}

unsigned int OpenRGBKeychronV6UltraPlugin::GetPluginAPIVersion()
{
    return(OPENRGB_PLUGIN_API_VERSION);
}

void OpenRGBKeychronV6UltraPlugin::Load(ResourceManagerInterface* resource_manager)
{
    rm = resource_manager;

    hid_init();

    for(const auto& device : KEYCHRON_DEVICES)
    {
        hid_device_info* devs = hid_enumerate(KEYCHRON_VID, device.pid);
        for(hid_device_info* cur = devs; cur != nullptr; cur = cur->next)
        {
            if(cur->usage_page != RAW_USAGE_PAGE || cur->usage != RAW_USAGE)
            {
                continue;                               /* only the raw command interface */
            }

            hid_device* dev = hid_open_path(cur->path);
            if(dev == nullptr)
            {
                continue;
            }

            KeychronV6UltraController* ctrl = new KeychronV6UltraController(dev, cur->path);
            if(!ctrl->IsOpenRGBFirmware())
            {
                delete ctrl;                            /* stock fw / not our device */
                continue;
            }

            RGBController_KeychronV6Ultra* rgb = new RGBController_KeychronV6Ultra(ctrl);
            rgb->name = device.name;                    /* V6 Ultra / Q6 Ultra */
            rm->RegisterRGBController(rgb);
            registered.push_back(rgb);
        }
        hid_free_enumeration(devs);
    }
}

QWidget* OpenRGBKeychronV6UltraPlugin::GetWidget()
{
    /*-----------------------------------------------------------------------*\
    | OpenRGB's OpenRGBPluginContainer does plugin_widget->setParent(this)    |
    | with NO null check, so a plugin MUST return a valid QWidget even when   |
    | it only registers a device. Return a small info label.                 |
    \*-----------------------------------------------------------------------*/
    QLabel* label = new QLabel(
        "Keychron V6 Ultra 8K (custom ZMK firmware)\n\n"
        "Control this keyboard from its device page: set the mode to \"Direct\" "
        "to drive the per-key RGB from OpenRGB.");
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setMargin(20);
    return(label);
}

QMenu* OpenRGBKeychronV6UltraPlugin::GetTrayMenu()
{
    return(nullptr);
}

void OpenRGBKeychronV6UltraPlugin::Unload()
{
    for(RGBController_KeychronV6Ultra* rgb : registered)
    {
        if(rm != nullptr)
        {
            rm->UnregisterRGBController(rgb);
        }
        delete rgb;                                     /* also closes the HID handle */
    }
    registered.clear();
}
