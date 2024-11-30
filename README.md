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

### What parameters are considered?
In order to determine the most appropriate transponder code for a specific flight, the server will use information from the plugin sent including:
* the controller call sign
* origin
* destination
* flight rule
* aircraft position
* the controller connection type

The server will identify the next available transponder code based on the airport (1st priority) and FIR (2nd priority) transponder code list, excluding:
* any non-discrete (ending with 00) codes
* any code already used by a pilot on the network
* any code already used and detected by a plugin user
* any code assigned to an aircraft and detected by a plugin user
If the aiport and FIR transponder codes are all exhausted, or if no matching entry in any of these lists can be found, a random transponder code outside the preferential range will be used.

### Sweatbox and local FSD Server
Requests of simulated aircraft (sweatbox session, or from your local FSD server) are flagged and will be handled separately by the server. This offers the same experience while not connecting to the live VATSIM network (although an internet connection is required for the communication to the CCAMS server). The plugin can therefore be used for training session on sweatbox without restrictions. The CCAMS server will make sure that requests from live and simulated traffic will not interfere with each other.

## EuroScope installation
* Load the plugin
* Navigate to the EuroScope general settings
	* Add ```1000``` to your (comma separated) list of "VFR squawks" (to suppress duplicate indications for transponder code 1000)
	* Set the string for "S-mode transponders" to ```HLEGWQS```
	* Remove the string of the field "Squawk code for S-mode transponders" (to ensure transponder code 1000 is only assigned by the plugin logic)
* Update your tag and list definitions and replace the EuroScope default tag items and functions by the ones of the plugin

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
* Mode S quawk error: Indicator of incorrect use or incorrect assignment of transponder code 1000. If such an incorrect use is detected, the field will display "MSSQ" in information colour.
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
* ```.ccams auto``` enables/disables automatic transponder code assignment for IFR airborne aircraft
* ```.ccams tracking``` enables/disables transponder code validation when starting to track a flight
* ```.ccams reload``` reloads local (refer to plugin settings) and remote (mode S capability and plugin version) config data

### Plugin settings
The Plug-in settings file (check the file location via EuroScope > other settings > Settings files setup) can be used to change some of the plugins default settings. Use the format ```CCAMS:[setting name]:[setting value]```.
* ```codeVFR```: your generic code to be assigned to VFR aircraft (default ```7000```)
* ```acceptFPLformatICAO```: to allow/ignore Mode S capabilities of flight plans with the equipment code specified according ICAO format (default ```1```)
* ```acceptFPLformatFAA```: to allow/ignore Mode S capabilities of flight plans with the equipment code specified according FAA format (default ```1```)
* ```AutoAssign```: to enable/disable the automatic transponder code assignment of IFR airborne aircraft (default setting ```1```)

If you operate in a region not equipped for transponder code ```1000``` or just do not wish to assign any transponder code ```1000```, set both ```acceptFPLformatICAO``` and ```acceptFPLformatFAA``` to ```0```. This will result in the plugin using discrete transponder codes only.

## Changes / Improvements / Reports
Local ops/tech staff may create an [issue](https://github.com/kusterjs/CCAMS/issues) on this GitHub to request a configuration change, to ensure compliance with regional and local transponder code ranges and schemes.

The current server configuration is available on https://ccams.kilojuliett.ch/.

All airports starting with one of the following combination of letters are considered Mode S capable:
```EB,ED,EH,EL,EP,ET,LD,LF,LH,LI,LK,LO,LR,LSZR,LSZB,LSZG,LSGC,LSZH,LSGG,LZ```, excluding ```EPCE,EPDA,EPDE,EPIR,EPKS,EPLK,EPLY,EPMB,EPMI,EPMM,EPOK,EPPR,EPPW,EPSN,EPTM```

## Credits
This plugin includes Mode S functionalities as introduced by the ModeS by Pierre Ferran [pierr3/ModeS](https://github.com/pierr3/ModeS). It's intending to replace any version of the ModeS plugin, also later versions maintained by Oliver Gruetzmann [ogruetzmann/ModeS](https://github.com/ogruetzmann/ModeS).