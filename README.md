# ddccli

Windows CLI utility for setting brightness/contrast on connected monitors.

````
Usage: ddccli.exe [options]
Utility for setting brightness/contrast on connected monitors via DDC/CI.

    -b, --brightness
        Sets monitor brightness
    -c, --contrast
        Sets monitor contrast
    -h, --help
        Prints this help message
    -l, --list
        Lists connected monitors
    -m, --monitor
        Selects a monitor to adjust. If not specified, actions affects all monitors.
````

# Building

## Requirements

* Visual Studio 2015/17
* ...or Visual C++ Build Tools

Run make.bat. `cl` needs to be in the path or must be executed using VS command prompt.
