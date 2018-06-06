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
#include <string.h>

#include <argagg.hpp>


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


void setMonitorBrightness (HANDLE hMonitor, unsigned long level)
{
    DWORD minimumBrightness;
    DWORD maximumBrightness;
    DWORD currentBrightness;

    if (!GetMonitorBrightness(hMonitor
          , &minimumBrightness
          , &currentBrightness
          , &maximumBrightness)) {
        throw std::runtime_error("failed to get monitor brightness");
        return;
    }

    DWORD newBrightness = static_cast<DWORD>(level);

    if (newBrightness < minimumBrightness){
        throw std::runtime_error("brightness level deceeds minimum");
    }
    if (newBrightness > maximumBrightness){
        throw std::runtime_error("brightness level exceeds maximum");
    }

    if (!SetMonitorBrightness(hMonitor, newBrightness)) {
        throw std::runtime_error("failed to set monitor brightness");
    }
}

unsigned long getMonitorBrightness (HANDLE hMonitor)
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

    return static_cast<unsigned long>(currentBrightness);
}

void setMonitorContrast (HANDLE hMonitor, unsigned long level)
{
    DWORD minimumContrast;
    DWORD maximumContrast;
    DWORD currentContrast;

    if (!GetMonitorContrast(hMonitor
          , &minimumContrast
          , &currentContrast
          , &maximumContrast)) {
        throw std::runtime_error("failed to get monitor contrast");
        return;
    }

    DWORD newContrast = static_cast<DWORD>(level);

    if (newContrast < minimumContrast){
        throw std::runtime_error("contrast level deceeds minimum");
    }
    if (newContrast > maximumContrast){
        throw std::runtime_error("contrast level exceeds maximum");
    }

    if (!SetMonitorContrast(hMonitor, newContrast)) {
        throw std::runtime_error("failed to set monitor contrast");
    }
}

unsigned long getMonitorContrast (HANDLE hMonitor)
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

    return static_cast<unsigned long>(currentContrast);
}



int main (int argc, char** argv)
{
    argagg::parser parser {{
        {
            "brightness", { "-b", "--brightness" }
          , "Sets monitor brightness", 1 }
      , {
            "contrast", { "-c", "--contrast" }
          , "Sets monitor contrast", 1 }
      , {
            "help", { "-h", "--help" }
          , "Prints this help message", 0 }
      , {
            "list", { "-l", "--list" }
          , "Lists connected monitors", 0 }
      , {
            "monitor", { "-m", "--monitor" }
          , "Selects a monitor to adjust. If not specified, actions affects all monitors.", 1 }
    }};

    try {

        argagg::parser_results args = parser.parse(argc, argv);

        // Help
        if (args["help"]) {
            argagg::fmt_ostream fmt(std::cout);

            fmt << "Usage: " << argv[0] << " [options]" << std::endl
                << "Utility for setting brightness/contrast on connected monitors via DDC/CI." << std::endl
                << std::endl
                << parser;

            return EXIT_SUCCESS;
        }

        std::map<std::string, HANDLE> monitorMap;
        EnumDisplayMonitors(NULL, NULL, &monitorEnumProc
              , reinterpret_cast<LPARAM>(&monitorMap));

        if (args["list"]) {
            for (auto const& monitor : monitorMap) {
                std::cout << monitor.first << std::endl;
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

        if (args["brightness"]) {
            unsigned long level = args["brightness"];
            for (auto const& monitor : monitorMap) {
                setMonitorBrightness(monitor.second, level);
            }
        }

        if (args["contrast"]) {
            unsigned long level = args["contrast"];
            for (auto const& monitor : monitorMap) {
                setMonitorContrast(monitor.second, level);
            }
        }

    } catch (const std::runtime_error e) {
        logError(e.what());
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        logError(e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
