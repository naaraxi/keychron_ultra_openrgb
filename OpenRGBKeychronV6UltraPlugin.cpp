/*---------------------------------------------------------*\
| OpenRGBKeychronV6UltraPlugin.cpp                          |
\*---------------------------------------------------------*/

#include "OpenRGBKeychronV6UltraPlugin.h"
#include "KeychronV6UltraController.h"
#include "KeychronLayouts.h"
#include <hidapi.h>
#include <QLabel>

#define KEYCHRON_VID        0x3434
#define RAW_USAGE_PAGE      0xFF60
#define RAW_USAGE           0x61

/*---------------------------------------------------------------------------*\
| The set of supported boards (PID + display name + LED layout) lives in       |
| KEYCHRON_LAYOUTS[] (KeychronLayouts.h). Every board speaks the same 0x16     |
| raw-HID protocol; only the PID, name and per-key layout differ.              |
\*---------------------------------------------------------------------------*/

OpenRGBPluginInfo OpenRGBKeychronV6UltraPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;
    info.Name          = "Keychron Ultra (OpenRGB direct)";
    info.Description    = "Direct per-key RGB control for Keychron V- and Q-series Ultra "
                          "keyboards running custom ZMK firmware (issue #893).";
    info.Version        = "0.3.0";
    info.Commit         = "";
    info.URL            = "https://github.com/naaraxi/keychron_ultra_openrgb";
    info.Location       = OPENRGB_PLUGIN_LOCATION_SETTINGS;
    info.Label          = "Keychron Ultra";
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

    for(unsigned int i = 0; i < KEYCHRON_LAYOUT_COUNT; i++)
    {
        const KeychronLayout* layout = &KEYCHRON_LAYOUTS[i];

        hid_device_info* devs = hid_enumerate(KEYCHRON_VID, layout->pid);
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

            /*---------------------------------------------------------------*\
            | Must speak our firmware AND report the LED count this layout    |
            | expects — guards against a PID/layout mismatch lighting wrong.  |
            \*---------------------------------------------------------------*/
            if(!ctrl->IsOpenRGBFirmware() || ctrl->GetLEDCount() != layout->led_count)
            {
                delete ctrl;
                continue;
            }

            RGBController_KeychronV6Ultra* rgb = new RGBController_KeychronV6Ultra(ctrl, layout);
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
        "Keychron Ultra series (custom ZMK firmware)\n\n"
        "Control your keyboard from its device page: set the mode to \"Direct\" "
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
