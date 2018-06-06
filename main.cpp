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


#ifndef NOMINMAX
#define NOMINMAX
#endif

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Dxva2.lib")

#include "windows.h"
#include "winuser.h"
#include "PhysicalMonitorEnumerationAPI.h"
#include "HighLevelMonitorConfigurationAPI.h"

#include <iostream>
#include <map>
#include <string>

#include <argagg.hpp>
#include <json.hpp>

using json = nlohmann::json;


void logError (const char* message)
{
    std::cerr << "error: " << message << std::endl;
}


BOOL CALLBACK monitorEnumProc (
        HMONITOR hMonitor
      , HDC hdcMonitor
      , LPRECT lprcMonitor
      , LPARAM dwData)
{
    MONITORINFOEX info;
    info.cbSize = sizeof(MONITORINFOEX);
    GetMonitorInfo(hMonitor, &info);

    DWORD numPhysicalMonitors;
    LPPHYSICAL_MONITOR physicalMonitors = NULL;

    if (!GetNumberOfPhysicalMonitorsFromHMONITOR(hMonitor
          , &numPhysicalMonitors)) {
        logError("failed to get number of physical monitors");
        return FALSE;
    }

    physicalMonitors = new PHYSICAL_MONITOR[numPhysicalMonitors];

    if (physicalMonitors != NULL) {
        if (!GetPhysicalMonitorsFromHMONITOR(hMonitor
              , numPhysicalMonitors, physicalMonitors)) {
            logError("failed to get physical monitors");
            return FALSE;
        }

        std::map<std::string, HANDLE>* monitorMap =
                reinterpret_cast<std::map<std::string, HANDLE>*>(dwData);

        monitorMap->insert(
              { static_cast<std::string>(info.szDevice)  // Monitor name
              , physicalMonitors[0].hPhysicalMonitor }); // Monitor handle

        /*if (!DestroyPhysicalMonitors(numPhysicalMonitors
              , physicalMonitors)) {
            std::cerr << "Failed to close monitor handles" << std::endl;
            return FALSE;
        }

        free(physicalMonitors);*/
        return TRUE;
    }

    return FALSE;
}


struct MonitorBrightness
{
    unsigned long minimumBrightness;
    unsigned long maximumBrightness;
    unsigned long currentBrightness;
};

struct MonitorContrast
{
    unsigned long minimumContrast;
    unsigned long maximumContrast;
    unsigned long currentContrast;
};

MonitorBrightness getMonitorBrightness (HANDLE hMonitor)
{
    DWORD minimumBrightness;
    DWORD maximumBrightness;
    DWORD currentBrightness;

    if (!GetMonitorBrightness(hMonitor
          , &minimumBrightness
          , &currentBrightness
          , &maximumBrightness)) {
        throw std::runtime_error("failed to get monitor brightness");
    }

    MonitorBrightness brightness = {
        static_cast<unsigned long>(minimumBrightness)
      , static_cast<unsigned long>(maximumBrightness)
      , static_cast<unsigned long>(currentBrightness)
    };

    return brightness;
}

MonitorContrast getMonitorContrast (HANDLE hMonitor)
{
    DWORD minimumContrast;
    DWORD maximumContrast;
    DWORD currentContrast;

    if (!GetMonitorContrast(hMonitor
          , &minimumContrast
          , &currentContrast
          , &maximumContrast)) {
        throw std::runtime_error("failed to get monitor contrast");
    }

    MonitorContrast contrast = {
        static_cast<unsigned long>(minimumContrast)
      , static_cast<unsigned long>(maximumContrast)
      , static_cast<unsigned long>(currentContrast)
    };

    return contrast;
}

void setMonitorBrightness (HANDLE hMonitor, unsigned long level)
{
    auto brightness = getMonitorBrightness(hMonitor);

    if (level < brightness.minimumBrightness){
        throw std::runtime_error("brightness level deceeds minimum");
    } else if (level > brightness.maximumBrightness){
        throw std::runtime_error("brightness level exceeds maximum");
    }

    if (!SetMonitorBrightness(hMonitor, static_cast<DWORD>(level))) {
        throw std::runtime_error("failed to set monitor brightness");
    }
}

void setMonitorContrast (HANDLE hMonitor, unsigned long level)
{
    auto contrast = getMonitorContrast(hMonitor);

    if (level < contrast.minimumContrast){
        throw std::runtime_error("contrast level deceeds minimum");
    } else if (level > contrast.maximumContrast){
        throw std::runtime_error("contrast level exceeds maximum");
    }

    if (!SetMonitorContrast(hMonitor, static_cast<DWORD>(level))) {
        throw std::runtime_error("failed to set monitor contrast");
    }
}


int main (int argc, char** argv)
{
    argagg::parser parser {{
        {
            "setBrightness", { "-b", "--brightness" }
          , "Sets monitor brightness", 1 }
      , {
            "getBrightness", { "-B", "--get-brightness" }
          , "Gets monitor brightness", 0 }
      , {
            "setContrast", { "-c", "--contrast" }
          , "Sets monitor contrast", 1 }
      , {
            "getContrast", { "-C", "--get-contrast" }
          , "Gets monitor contrast", 0 }
      , {
            "help", { "-h", "--help" }
          , "Prints this help message", 0 }
      , {
            "version", { "-v", "--version" }
          , "Prints the version number", 0 }
      , {
            "list", { "-l", "--list" }
          , "Lists connected monitors", 0 }
      , {
            "monitor", { "-m", "--monitor" }
          , "Selects a monitor to adjust. If not specified, actions affects all monitors.", 1 }
      , {
            "json", {"-j", "--json"}
          , "Outputs action results as JSON", 0}
    }};

    std::string versionString = "v0.0.2";

    std::ostringstream usage;
    usage
        << argv[0] << " " << versionString << std::endl
        << "Usage: "<< argv[0] << " [options]" << std::endl;

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
                << "Copyright (c) 2018 Matt Hensman <m@matt.tf>" << std::endl
                << "MIT License" << std::endl;

            return EXIT_SUCCESS;
        }

        bool shouldOutputJson = false;
        json jsonOutput;
        if (args["json"]) {
            shouldOutputJson = true;
        }

        try {
            std::map<std::string, HANDLE> monitorMap;
            EnumDisplayMonitors(NULL, NULL, &monitorEnumProc
                  , reinterpret_cast<LPARAM>(&monitorMap));

            if (args["list"]) {
                if (shouldOutputJson) {
                    jsonOutput["monitorList"] = json::array();
                }

                for (auto const& monitor : monitorMap) {
                    if (shouldOutputJson) {
                        jsonOutput["monitorList"].push_back(monitor.first);
                    } else {
                        std::cout << monitor.first << std::endl;
                    }
                }
            }


            if (args["monitor"]) {
                std::string selectedMonitorName = args["monitor"];

                // Remove all non-matching monitors from the map
                for (auto const& monitor : monitorMap) {
                    if (monitor.first != selectedMonitorName) {
                        monitorMap.erase(monitor.first);
                    }
                }

                if (monitorMap.empty()) {
                    throw std::runtime_error("monitor doesn't exist");
                }
            }

            if (args["setBrightness"]) {
                unsigned long level = args["setBrightness"];
                for (auto const& monitor : monitorMap) {
                    setMonitorBrightness(monitor.second, level);
                }
            }

            if (args["getBrightness"]) {
                if (args["monitor"]) {
                    auto it = monitorMap.find(args["monitor"]);
                    if (it == monitorMap.end()) {
                        throw std::runtime_error("monitor not found");
                    }

                    auto brightness = getMonitorBrightness(it->second).currentBrightness;

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
                for (auto const& monitor : monitorMap) {
                    setMonitorContrast(monitor.second, level);
                }
            }

            if (args["getContrast"]) {
                if (args["monitor"]) {
                    auto it = monitorMap.find(args["monitor"]);
                    if (it == monitorMap.end()) {
                        throw std::runtime_error("monitor not found");
                    }

                    auto contrast = getMonitorContrast(it->second).currentContrast;

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
        std::cerr
            << "Error parsing arguments: " << e.what() << std::endl
            << usage.str() << parser;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
