# ddccli

Windows CLI utility for setting brightness/contrast on connected monitors.

````
Usage: ddccli.exe [options]
Utility for setting brightness/contrast on connected monitors via DDC/CI.

    -b, --brightness
        Sets monitor brightness
    -B, --get-brightness
        Gets monitor brightness
    -c, --contrast
        Sets monitor contrast
    -C, --get-contrast
        Gets monitor contrast
    -h, --help
        Prints this help message
    -v, --version
        Prints the version number
    -l, --list
        Lists connected monitors
    -m, --monitor
        Selects a monitor to adjust. If not specified, actions affects all monitors.
````

# Building

## Requirements

* Visual Studio 2019
* ...or Visual C++ Build Tools

Open solution in VS and build from there or via the [command line](https://docs.microsoft.com/en-us/cpp/build/msbuild-visual-cpp?view=msvc-160).
