/*---------------------------------------------------------*\
| OpenRGBKeychronV6UltraPlugin.cpp                          |
\*---------------------------------------------------------*/

#include "OpenRGBKeychronV6UltraPlugin.h"
#include "KeychronV6UltraController.h"
#include "KeychronLayouts.h"
#include <algorithm>
#include <hidapi.h>
#include <QLabel>

/*---------------------------------------------------------------------------*\
| VID and the raw-interface usage filter come from KeychronV6UltraController.h  |
| so that detection here and the controller's reconnect path can never drift    |
| apart and match different interfaces.                                        |
\*---------------------------------------------------------------------------*/

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
    info.Version        = "0.4.1";
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

    DetectControllers();

    /*-----------------------------------------------------------------------*\
    | OpenRGB loads plugins exactly once per run - OpenRGBDialog's            |
    | onDetectionEnded() gates ScanAndLoadPlugins() behind a plugins_loaded   |
    | flag - but every rescan calls ResourceManager::Cleanup(), which deletes |
    | every controller in rgb_controllers_hw. RegisterRGBController() puts    |
    | ours in that same list, so a rescan destroys our devices and nothing    |
    | ever asks the plugin for them again: the keyboard silently disappears   |
    | for the rest of the session, and `registered` is left holding freed     |
    | pointers that Unload() would unregister and delete a second time.       |
    |                                                                        |
    | Detection-end is the right place to rebuild: Cleanup() has run, the     |
    | resource manager's own detection has finished writing the device list,  |
    | and it fires on the initial detection as well as on every rescan.       |
    \*-----------------------------------------------------------------------*/
    rm->RegisterDetectionEndCallback(&OpenRGBKeychronV6UltraPlugin::OnDetectionEnd, this);
}

void OpenRGBKeychronV6UltraPlugin::OnDetectionEnd(void* arg)
{
    OpenRGBKeychronV6UltraPlugin* plugin = (OpenRGBKeychronV6UltraPlugin*)arg;

    plugin->DropDeletedControllers();
    plugin->DetectControllers();
}

/*---------------------------------------------------------------------------*\
| Forget the controllers OpenRGB has already destroyed, WITHOUT deleting them: |
| Cleanup() owns and frees them. A stale entry left here would be passed to    |
| UnregisterRGBController() - which dereferences it for logging and for the    |
| virtual ClearCallbacks() call - and then deleted again.                      |
|                                                                             |
| Liveness is decided by pointer identity against the resource manager's list, |
| which Cleanup() erases ours from, so no freed pointer is ever dereferenced.  |
\*---------------------------------------------------------------------------*/
void OpenRGBKeychronV6UltraPlugin::DropDeletedControllers()
{
    if(rm == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(registered_mutex);

    const std::vector<RGBController*>&          live = rm->GetRGBControllers();
    std::vector<RGBController_KeychronV6Ultra*> alive;

    for(RGBController_KeychronV6Ultra* rgb : registered)
    {
        if(std::find(live.begin(), live.end(), (RGBController*)rgb) != live.end())
        {
            alive.push_back(rgb);
        }
    }

    registered = alive;
}

void OpenRGBKeychronV6UltraPlugin::DetectControllers()
{
    std::lock_guard<std::mutex> lock(registered_mutex);

    /*-----------------------------------------------------------------------*\
    | Our devices survived the detection cycle (no Cleanup(), e.g. detection  |
    | disabled, or a rescan that bailed out early), so re-registering would   |
    | just duplicate them and open a second handle to the same keyboard.      |
    \*-----------------------------------------------------------------------*/
    if(!registered.empty())
    {
        return;
    }

    for(unsigned int i = 0; i < KEYCHRON_LAYOUT_COUNT; i++)
    {
        const KeychronLayout* layout = &KEYCHRON_LAYOUTS[i];

        hid_device_info* devs = hid_enumerate(KEYCHRON_V6U_VID, layout->pid);
        for(hid_device_info* cur = devs; cur != nullptr; cur = cur->next)
        {
            if(cur->usage_page != KEYCHRON_V6U_RAW_USAGE_PAGE
            || cur->usage      != KEYCHRON_V6U_RAW_USAGE)
            {
                continue;                               /* only the raw command interface */
            }

            hid_device* dev = hid_open_path(cur->path);
            if(dev == nullptr)
            {
                continue;
            }

            KeychronV6UltraController* ctrl = new KeychronV6UltraController(dev, cur->path,
                                                                           layout->pid,
                                                                           layout->led_count);

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
    /*-----------------------------------------------------------------------*\
    | Stop the callback first. UnloadPlugins() can dlclose this plugin, and a |
    | detection cycle afterwards would call into unmapped code.               |
    \*-----------------------------------------------------------------------*/
    if(rm != nullptr)
    {
        rm->UnregisterDetectionEndCallback(&OpenRGBKeychronV6UltraPlugin::OnDetectionEnd, this);
    }

    /*-----------------------------------------------------------------------*\
    | Anything a rescan already destroyed is not ours to unregister or free.  |
    | Called before taking the lock below - it takes the same one.            |
    \*-----------------------------------------------------------------------*/
    DropDeletedControllers();

    std::lock_guard<std::mutex> lock(registered_mutex);

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
