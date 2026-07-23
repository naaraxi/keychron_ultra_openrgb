/*---------------------------------------------------------*\
| OpenRGBKeychronV6UltraPlugin.h                            |
|                                                           |
|   OpenRGB plugin that registers the Keychron V6 Ultra 8K  |
|   (custom ZMK firmware, OpenRGB direct control) as an      |
|   OpenRGB device. API v4 (OpenRGB 1.0rc3).                |
\*---------------------------------------------------------*/

#pragma once

#include <QObject>
#include <QWidget>
#include <QMenu>
#include <vector>

#include "OpenRGBPluginInterface.h"
#include "ResourceManagerInterface.h"
#include "RGBController_KeychronV6Ultra.h"

class OpenRGBKeychronV6UltraPlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID)
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    OpenRGBPluginInfo   GetPluginInfo()                                    override;
    unsigned int        GetPluginAPIVersion()                             override;
    void                Load(ResourceManagerInterface* resource_manager)  override;
    QWidget*            GetWidget()                                        override;
    QMenu*              GetTrayMenu()                                      override;
    void                Unload()                                          override;

private:
    ResourceManagerInterface*                    rm = nullptr;
    std::vector<RGBController_KeychronV6Ultra*>  registered;
};
