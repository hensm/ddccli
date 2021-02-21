/*

Copyright (c) 2018 Matt Hensman <m@matt.tf>

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

#include "HighLevelMonitorConfigurationAPI.h"
#include "PhysicalMonitorEnumerationAPI.h"
#include "windows.h"
#include "winuser.h"

#include <iostream>
#include <map>
#include <string>

#include <argagg.hpp>
#include <json.hpp>

using json = nlohmann::json;


void
logError(const char* message)
{
    std::cerr << "error: " << message << std::endl;
}


std::map<std::string, HANDLE> handles;

void
populateHandlesMap()
{
    // Cleanup
    if (!handles.empty()) {
        for (auto const& handle : handles) {
            DestroyPhysicalMonitor(handle.second);
        }
        handles.clear();
    }


    struct Monitor {
        HMONITOR handle;
        std::vector<HANDLE> physicalHandles;
    };

    auto monitorEnumProc = [](HMONITOR hMonitor,
                              HDC hdcMonitor,
                              LPRECT lprcMonitor,
                              LPARAM dwData) -> BOOL {
        auto monitors = reinterpret_cast<std::vector<struct Monitor>*>(dwData);
        monitors->push_back({ hMonitor, {} });
        return TRUE;
    };

    std::vector<struct Monitor> monitors;
    EnumDisplayMonitors(
      NULL, NULL, monitorEnumProc, reinterpret_cast<LPARAM>(&monitors));

    // Get physical monitor handles
    for (auto& monitor : monitors) {
        DWORD numPhysicalMonitors;
        LPPHYSICAL_MONITOR physicalMonitors = NULL;
        if (!GetNumberOfPhysicalMonitorsFromHMONITOR(monitor.handle,
                                                     &numPhysicalMonitors)) {
            throw std::runtime_error("Failed to get physical monitor count.");
            exit(EXIT_FAILURE);
        }

        physicalMonitors = new PHYSICAL_MONITOR[numPhysicalMonitors];
        if (physicalMonitors == NULL) {
            throw std::runtime_error(
              "Failed to allocate physical monitor array");
        }

        if (!GetPhysicalMonitorsFromHMONITOR(
              monitor.handle, numPhysicalMonitors, physicalMonitors)) {
            throw std::runtime_error("Failed to get physical monitors.");
        }

        for (DWORD i = 0; i <= numPhysicalMonitors; i++) {
            monitor.physicalHandles.push_back(
              physicalMonitors[(numPhysicalMonitors == 1 ? 0 : i)]
                .hPhysicalMonitor);
        }

        delete[] physicalMonitors;
    }


    DISPLAY_DEVICE adapterDev;
    adapterDev.cb = sizeof(DISPLAY_DEVICE);

    // Loop through adapters
    int adapterDevIndex = 0;
    while (EnumDisplayDevices(NULL, adapterDevIndex++, &adapterDev, 0)) {
        DISPLAY_DEVICE displayDev;
        displayDev.cb = sizeof(DISPLAY_DEVICE);

        // Loop through displays (with device ID) on each adapter
        int displayDevIndex = 0;
        while (EnumDisplayDevices(adapterDev.DeviceName,
                                  displayDevIndex++,
                                  &displayDev,
                                  EDD_GET_DEVICE_INTERFACE_NAME)) {

            // Check valid target
            if (!(displayDev.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
                || displayDev.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) {
                continue;
            }

            for (auto const& monitor : monitors) {
                MONITORINFOEX monitorInfo;
                monitorInfo.cbSize = sizeof(MONITORINFOEX);
                GetMonitorInfo(monitor.handle, &monitorInfo);

                for (size_t i = 0; i < monitor.physicalHandles.size(); i++) {
                    /**
                     * Re-create DISPLAY_DEVICE.DeviceName with
                     * MONITORINFOEX.szDevice and monitor index.
                     */
                    std::string monitorName =
                      static_cast<std::string>(monitorInfo.szDevice)
                      + "\\Monitor" + std::to_string(i);

                    std::string deviceName =
                      static_cast<std::string>(displayDev.DeviceName);

                    // Match and store against device ID
                    if (monitorName == deviceName) {
                        handles.insert(
                          { static_cast<std::string>(displayDev.DeviceID),
                            monitor.physicalHandles[i] });

                        break;
                    }
                }
            }
        }
    }
}



struct MonitorBrightness {
    unsigned long maximumBrightness;
    unsigned long currentBrightness;
};

struct MonitorContrast {
    unsigned long maximumContrast;
    unsigned long currentContrast;
};

MonitorBrightness
getMonitorBrightness(HANDLE hMonitor)
{
    DWORD minimumBrightness;
    DWORD maximumBrightness;
    DWORD currentBrightness;

    if (!GetMonitorBrightness(hMonitor,
                              &minimumBrightness,
                              &currentBrightness,
                              &maximumBrightness)) {
        throw std::runtime_error("failed to get monitor brightness");
    }

    MonitorBrightness brightness = {
        static_cast<unsigned long>(maximumBrightness),
        static_cast<unsigned long>(currentBrightness)
    };

    return brightness;
}

MonitorContrast
getMonitorContrast(HANDLE hMonitor)
{
    DWORD minimumContrast;
    DWORD maximumContrast;
    DWORD currentContrast;

    if (!GetMonitorContrast(
          hMonitor, &minimumContrast, &currentContrast, &maximumContrast)) {
        throw std::runtime_error("failed to get monitor contrast");
    }

    MonitorContrast contrast = { static_cast<unsigned long>(maximumContrast),
                                 static_cast<unsigned long>(currentContrast) };

    return contrast;
}

void
setMonitorBrightness(HANDLE hMonitor, unsigned long level)
{
    auto brightness = getMonitorBrightness(hMonitor);

    if (level > brightness.maximumBrightness) {
        throw std::runtime_error("brightness level exceeds maximum");
    }

    if (!SetMonitorBrightness(hMonitor, static_cast<DWORD>(level))) {
        throw std::runtime_error("failed to set monitor brightness");
    }
}

void
setMonitorContrast(HANDLE hMonitor, unsigned long level)
{
    auto contrast = getMonitorContrast(hMonitor);

    if (level > contrast.maximumContrast) {
        throw std::runtime_error("contrast level exceeds maximum");
    }

    if (!SetMonitorContrast(hMonitor, static_cast<DWORD>(level))) {
        throw std::runtime_error("failed to set monitor contrast");
    }
}


int
main(int argc, char** argv)
{
    argagg::parser parser{
        { { "setBrightness",
            { "-b", "--brightness" },
            "Sets monitor brightness",
            1 },
          { "getBrightness",
            { "-B", "--get-brightness" },
            "Gets monitor brightness",
            0 },
          { "setContrast", { "-c", "--contrast" }, "Sets monitor contrast", 1 },
          { "getContrast",
            { "-C", "--get-contrast" },
            "Gets monitor contrast",
            0 },
          { "help", { "-h", "--help" }, "Prints this help message", 0 },
          { "version", { "-v", "--version" }, "Prints the version number", 0 },
          { "list", { "-l", "--list" }, "Lists connected monitors", 0 },
          { "monitor",
            { "-m", "--monitor" },
            "Selects a monitor to adjust. If not specified, actions affects all monitors.",
            1 },
          { "json", { "-j", "--json" }, "Outputs action results as JSON", 0 } }
    };

    std::string versionString = "v0.1.0";

    std::ostringstream usage;
    usage << argv[0] << " " << versionString << std::endl
          << "Usage: " << argv[0] << " [options]" << std::endl;

    try {

        argagg::parser_results args = parser.parse(argc, argv);

        // Help
        if (args["help"]) {
            std::cout << usage.str() << parser;
            return EXIT_SUCCESS;
        }

        if (args["version"]) {
            argagg::fmt_ostream fmt(std::cout);

            fmt << "ddccli " << versionString << std::endl
                << "Copyright (c) 2021 Matt Hensman <m@matt.tf>" << std::endl
                << "MIT License" << std::endl;

            return EXIT_SUCCESS;
        }

        bool shouldOutputJson = false;
        json jsonOutput;
        if (args["json"]) {
            shouldOutputJson = true;
        }

        try {
            populateHandlesMap();

            if (args["list"]) {
                if (shouldOutputJson) {
                    jsonOutput["monitorList"] = json::array();
                }

                for (auto const& [ id, handle ] : handles) {
                    if (shouldOutputJson) {
                        jsonOutput["monitorList"].push_back(id);
                    } else {
                        std::cout << id << std::endl;
                    }
                }
            }


            if (args["monitor"]) {
                std::string selectedMonitorName = args["monitor"];

                // Remove all non-matching monitors from the map
                for (auto const& [ id, handle ] : handles) {
                    if (id != selectedMonitorName) {
                        handles.erase(id);
                    }
                }

                if (handles.empty()) {
                    throw std::runtime_error("monitor doesn't exist");
                }
            }

            if (args["setBrightness"]) {
                unsigned long level = args["setBrightness"];
                for (auto const& [ id, handle ] : handles) {
                    setMonitorBrightness(handle, level);
                }
            }

            if (args["getBrightness"]) {
                if (args["monitor"]) {
                    auto it = handles.find(args["monitor"]);
                    if (it == handles.end()) {
                        throw std::runtime_error("monitor not found");
                    }

                    auto brightness =
                      getMonitorBrightness(it->second).currentBrightness;

                    if (shouldOutputJson) {
                        jsonOutput["brightness"] = brightness;
                    } else {
                        std::cout << brightness << std::endl;
                    }
                } else {
                    throw std::runtime_error(
                      "no monitor specified to query brightness");
                }
            }

            if (args["setContrast"]) {
                unsigned long level = args["setContrast"];
                for (auto const& [ id, handle ] : handles) {
                    setMonitorContrast(handle, level);
                }
            }

            if (args["getContrast"]) {
                if (args["monitor"]) {
                    auto it = handles.find(args["monitor"]);
                    if (it == handles.end()) {
                        throw std::runtime_error("monitor not found");
                    }

                    auto contrast =
                      getMonitorContrast(it->second).currentContrast;

                    if (shouldOutputJson) {
                        jsonOutput["contrast"] = contrast;
                    } else {
                        std::cout << contrast << std::endl;
                    }
                } else {
                    throw std::runtime_error(
                      "no monitor specified to query contrast");
                }
            }
        } catch (const std::runtime_error e) {
            logError(e.what());
            return EXIT_FAILURE;
        }

        if (shouldOutputJson) {
            std::cout << jsonOutput << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error parsing arguments: " << e.what() << std::endl
                  << usage.str() << parser;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
