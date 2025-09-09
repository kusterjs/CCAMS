# CCAMS (Centralised code assignment and management system) plugin

This plugin was designed to offer controllers of the VATSIM network an enhanced possibility for assigning transponder codes (squawks) in a coordinated and consistent manner, including enhanced Mode S functionalities.

Find the latest version in the section [Releases](https://github.com/kusterjs/CCAMS/releases) (see right menu).

## Issue
Increasing traffic volumes and limited functionalities of the controller clients cause:
* shortage and exhausted local transponder code ranges
* duplicate use of transponder codes
* inconsistent use and assignment of code 1000

## Solution
A client-server solution is used to:
* manage all requests for transponder codes at a central interface
* keep a single list of reserved codes
* using additional information that is not available to controller clients
* ensure consistency also among different software releases of the plugin

The plugin sends requests to the server acting as the central interface to manage all transponder code assignments. It will evaluate any request received via the plugin based on the configuration of FIR and airport code ranges. You can review the current configuration status and the latest usage statistics on https://ccams.kilojuliett.ch/.

## EuroScope installation
* Load the plugin
* Navigate to the EuroScope general settings
	* Add ```1000``` to your (comma separated) list of "VFR squawks" (to suppress duplicate indications for transponder code 1000)
	* Set the string for "S-mode transponders" to ```HLEGWQS```
	* Remove the string of the field "Squawk code for S-mode transponders" (to ensure transponder code 1000 is only assigned by the plugin logic)
* Update your tag and list definitions and replace the EuroScope default tag items and functions by the ones of the plugin

All airports starting with one of the following combination of letters are considered Mode S capable:
```EB,ED,EH,EL,EP,ET,LD,LF,LH,LI,LK,LO,LR,LSZR,LSZB,LSZG,LSGC,LSZH,LSGG,LZ```, excluding ```EPCE,EPDA,EPDE,EPIR,EPKS,EPLK,EPLY,EPMB,EPMI,EPMM,EPOK,EPPR,EPPW,EPSN,EPTM```

## Compatibility

### EuroScope
This plugin provides full compatibility with EuroScope until version 3.2.3.

EuroScope version 3.2.4 and later do not correctly report the aircraft equipment (as provided by the pilot via the flight plan), which will result in no Mode S compatibility detected for any aircraft. You can still use the plugin with these versions, but discrete codes will be assigned in all cases. It is recommended to deactivate the automatic transponder code assignment. A corresponding message will be raised by the plugin upon startup.

### Sweatbox and local FSD Server
Requests of simulated aircraft (sweatbox session, or from your local FSD server) are flagged and will be handled separately by the server. This offers the same experience while not connecting to the live VATSIM network (although an internet connection is required for the communication to the CCAMS server). The plugin can therefore be used for training session on sweatbox without restrictions. The CCAMS server will make sure that requests from live and simulated traffic will not interfere with each other.

## Functionalities
This plugin provides capabilities/functionalities to:
* interpret both the FAA and ICAO flight plan format
* assign transponder codes manually
* assign transponder codes automatically, if
	* flight plan rule is IFR
	* traffic is airborne
	* traffic is not in the vicinity of the arrival or departure airport
	* the controller is logged in as APP, CTR or FSS
* customise its behaviour
* limit certain functionalities
* monitor (simulated) Mode S EHS (Enhanced Surveillance) data
* detect simulated aircraft (sweatbox)

### Tag items
* Assigned squawk: the assigned transponder code, includes colour indication for:
	* disagreement between assigned and set transponder code (information colour)
	* incorrect use or assignment of transponder code 1000 (redundant colour)
* Mode S quawk error: Indicator of incorrect use or incorrect assignment of transponder code ```1000```. If such an incorrect use is detected, the field will display ```MSSQ``` in information colour.
* Transponder type: displays ```S``` for Mode S equipped aircraft, otherwise ```A```
* EHS Heading: Reported magnetic heading of the aircraft
* EHS Roll Angle: Reported roll angle (```L``` for LEFT and ```R``` for RIGHT + value in degrees)
* EHS GS: Reported groundspeed of the aircraft in knots.

### Tag functions
* Auto assign squawk: primary function of the plugin, assigns an appropriate transponder code considering all available data (controller data, flight plan data, server configuration)
* Open SQUAWK assign popup: replacement for the EuroScope default tag function, with additional options:
	* Discrete: Assigns a discrete transponder code based on the server configuration, ignoring Mode S capabilities
	* VFR: Assigns an applicable VFR transponder code, based on the plugin settings and the server configuration

### Lists
* Mode S EHS: Displays a list to display Mode S EHS (Enhanced Surveillance) data of the currently selected aircraft

### Commands
* ```.help ccams``` provides a list of all available plugin commands
* ```.ccams ehslist``` displays the Mode S EHS list
* ```.ccams auto [seconds]``` enables/disables automatic transponder code assignment for IFR airborne aircraft, the optional value sets the refresh rate (in seconds) to scan for new assignments
* ```.ccams tracking``` enables/disables transponder code validation when starting to track a flight
* ```.ccams reload``` reloads local (refer to plugin settings) and remote (mode S capability and plugin version) config data

### Settings
The Plug-in settings file (check the file location via EuroScope > other settings > Settings files setup) can be used to change some of the plugins default settings. Use the format ```CCAMS:[setting name]:[setting value]```.
* ```codeVFR```: your generic code to be assigned to VFR aircraft (default ```7000```)
* ```acceptFPLformatICAO```: to allow/ignore Mode S capabilities of flight plans with the equipment code specified according ICAO format (default ```1```)
* ```acceptFPLformatFAA```: to allow/ignore Mode S capabilities of flight plans with the equipment code specified according FAA format (default ```1```)
* ```AutoAssign```: the refresh rate (in seconds) to search and perform automatic transponder code assignment of IFR airborne aircraft (default setting ```10```, deactivate with ```0```)
* ```tagColour```: a custom colour for the Mode S EHS list items as hex value (the colour of the default tag items like the call sign needs to be changed in the tag editor), default tag colour will be used if no value is set

If you operate in a region not equipped for transponder code ```1000``` or just do not wish to assign any transponder code ```1000```, set both ```acceptFPLformatICAO``` and ```acceptFPLformatFAA``` to ```0```. This will result in the plugin using discrete transponder codes only.

## Changes / Improvements / Reports
For any kind of report, please open an [issue](https://github.com/kusterjs/CCAMS/issues) for this repo.

### Assigned Transponder Codes Configuration
The assigned transponder code (ranges) are a matter of the server configuration. For more details, please visit the [CCAMS Server](https://github.com/kusterjs/CCAMS-server) repo.

## Credits
This plugin includes Mode S functionalities as introduced by the ModeS by Pierre Ferran [pierr3/ModeS](https://github.com/pierr3/ModeS). It's intending to replace any version of the ModeS plugin, also later versions maintained by Oliver Gruetzmann [ogruetzmann/ModeS](https://github.com/ogruetzmann/ModeS).

Thanks to [@herver](https://github.com/herver) for the contribution https://github.com/kusterjs/CCAMS/pull/52 to improve the compatibility on non-Windows plattforms (through Wine etc.).