#include "stdafx.h"
#include "CCAMS.h"
#include "version.h"
#include <fstream>

CCAMS::CCAMS(const EquipmentCodes&& ec, const SquawkCodes&& sc, const ModeS&& ms) : CPlugIn(EuroScopePlugIn::COMPATIBILITY_CODE,
	MY_PLUGIN_NAME,
	VER_FILEVERSION_STR,
	MY_PLUGIN_DEVELOPER,
	MY_PLUGIN_COPYRIGHT),
	EquipmentCodesFAA(ec.FAA),
	EquipmentCodesICAO(ec.ICAO_MODE_S),
	EquipmentCodesICAOEHS(ec.ICAO_EHS),
	ModeSAirports(ms.AIRPORTS_MATCH),
	ModeSAirportsExcl(ms.AIRPORTS_NOTMATCH),
	ModeSRoute(ms.ROUTE_MATCH),
	ModeSRouteExcl(ms.ROUTE_NOTMATCH),
	squawkModeS(sc.MODE_S),
	squawkVFR(sc.VFR)
{
	string DisplayMsg { "Version " + string { VER_FILEVERSION_STR } + " loaded" };
#ifdef _DEBUG
	DisplayUserMessage(MY_PLUGIN_NAME, "Initialisation", ("DEBUG " + DisplayMsg).c_str(), true, false, false, false, false);
#else
	DisplayUserMessage(MY_PLUGIN_NAME, "Initialisation", DisplayMsg.c_str(), true, false, false, false, false);
#endif

	RegisterTagItemType("Transponder Type", ItemCodes::TAG_ITEM_ISMODES);
	RegisterTagItemType("EHS Heading", ItemCodes::TAG_ITEM_EHS_HDG);
	RegisterTagItemType("EHS Roll Angle", ItemCodes::TAG_ITEM_EHS_ROLL);
	RegisterTagItemType("EHS GS", ItemCodes::TAG_ITEM_EHS_GS);
	RegisterTagItemType("Mode S squawk error", ItemCodes::TAG_ITEM_ERROR_MODES_USE);
	RegisterTagItemType("Assigned squawk", ItemCodes::TAG_ITEM_SQUAWK);
	RegisterTagItemType("Pinned to EHS list", ItemCodes::TAG_ITEM_EHS_PINNED);

	RegisterTagItemFunction("Auto assign squawk", ItemCodes::TAG_FUNC_ASSIGN_SQUAWK_AUTO);
	RegisterTagItemFunction("Open SQUAWK assign popup", ItemCodes::TAG_FUNC_SQUAWK_POPUP);
	RegisterTagItemFunction("Toggle pin to EHS list", ItemCodes::TAG_FUNC_TOGGLE_EHS_LIST);

	FpListEHS = RegisterFpList("Mode S EHS");
	if (FpListEHS.GetColumnNumber() == 0)
	{
		FpListEHS.AddColumnDefinition("P", 1, false, MY_PLUGIN_NAME, ItemCodes::TAG_ITEM_EHS_PINNED, MY_PLUGIN_NAME, ItemCodes::TAG_FUNC_TOGGLE_EHS_LIST, NULL, NULL);
		FpListEHS.AddColumnDefinition("C/S", 8, false, NULL, TAG_ITEM_TYPE_CALLSIGN, NULL, TAG_ITEM_FUNCTION_HANDOFF_POPUP_MENU, NULL, TAG_ITEM_FUNCTION_OPEN_FP_DIALOG);
		FpListEHS.AddColumnDefinition("HDG", 5, true, MY_PLUGIN_NAME, ItemCodes::TAG_ITEM_EHS_HDG, NULL, NULL, NULL, NULL);
		FpListEHS.AddColumnDefinition("Roll", 5, true, MY_PLUGIN_NAME, ItemCodes::TAG_ITEM_EHS_ROLL, NULL, NULL, NULL, NULL);
		FpListEHS.AddColumnDefinition("GS", 4, true, MY_PLUGIN_NAME, ItemCodes::TAG_ITEM_EHS_GS, NULL, NULL, NULL, NULL);
	}

	// Start new thread to get the version file from the server
	fVersion = async_http_get(MY_PLUGIN_UPDATE_BASE, MY_PLUGIN_UPDATE_ENDPOINT);
	fConfig = async_http_get(MY_PLUGIN_CONFIG_BASE, MY_PLUGIN_CONFIG_ENDPOINT);

	// Set default setting values
	ConnectionState = 0;
	RemoteConnectionState = 0;
	pluginVersionCheck = false;
	acceptEquipmentICAO = true;
	acceptEquipmentFAA = true;
#ifdef _DEBUG
	autoAssign = 3;
#else
	autoAssign = 10;
#endif
	APTcodeMaxGS = 50;
	APTcodeMaxDist = 3;
	tagColour = CLR_INVALID;
	
	ReadSettings();
}

CCAMS::~CCAMS()
{}

bool CCAMS::OnCompileCommand(const char* sCommandLine)
{
	string commandString(sCommandLine);
	cmatch matches;

	if (_stricmp(sCommandLine, ".help") == 0)
	{
		DisplayUserMessage("HELP", "HELP", ".HELP CCAMS | Centralised code assignment and management system Help", true, true, true, true, false);
		return NULL;
	}
	else if (_stricmp(sCommandLine, ".help ccams") == 0)
	{
		// Display HELP
		DisplayUserMessage("HELP", MY_PLUGIN_NAME, ".CCAMS EHSLIST | Displays the flight plan list with EHS values of the currently selected aircraft.", true, true, true, true, false);
		DisplayUserMessage("HELP", MY_PLUGIN_NAME, ".CCAMS AUTO [SECONDS] | Activates or deactivates automatic code assignment. Optionally, the refresh rate (in seconds) can be customised.", true, true, true, true, false);
		DisplayUserMessage("HELP", MY_PLUGIN_NAME, ".CCAMS TRACKING | Activates or deactivates transponder code validation when starting to track a flight.", true, true, true, true, false);
		DisplayUserMessage("HELP", MY_PLUGIN_NAME, ".CCAMS RELOAD | Force load of local and remote settings.", true, true, true, true, false);
#ifdef _DEBUG
		DisplayUserMessage("HELP", MY_PLUGIN_NAME, ".CCAMS RESET | Clears the list of flight plans which have been determined no longer applicable for automatic code assignment.", true, true, true, true, false);
		DisplayUserMessage("HELP", MY_PLUGIN_NAME, ".CCAMS [CALL SIGN] | Displays tracking and controller information for a specific flight (to support debugging of automatic code assignment).", true, true, true, true, false);
#endif
		return true;
	}
	else if (regex_search(sCommandLine, matches, regex("^\\.ccams\\s+(\\w+)(?:\\s+(\\d+))?", regex::icase)))
	{
		return PluginCommands(matches);
	}

	return false;
}

bool CCAMS::PluginCommands(cmatch Command)
{
	string sCommand = Command[1].str();
	if (sCommand == "ehslist")
	{
		FpListEHS.ShowFpList(true);
		return true;
	}
	else if (sCommand == "auto")
	{
		int autoAssignRefreshRate = 0;
		if (Command[2].str().length() > 0) autoAssignRefreshRate = stoi(Command[2].str());

		if (!pluginVersionCheck)
		{
			DisplayUserMessage(MY_PLUGIN_NAME, "Error", "Your plugin version is not up-to-date and the automatic code assignment therefore not available.", true, true, false, false, false);
		}
		else if (autoAssignRefreshRate > 0)
		{
			autoAssign = autoAssignRefreshRate;
			SaveDataToSettings("AutoAssign", "Automatic assignment of squawk codes", to_string(autoAssignRefreshRate).c_str());
			DisplayUserMessage(MY_PLUGIN_NAME, "Setting changed", string("Automatic code assignment enabled (refresh rate " + to_string(autoAssignRefreshRate) + " seconds)").c_str(), true, true, false, false, false);
		}
		else if (autoAssign > 0 || autoAssignRefreshRate == 0)
		{
			autoAssign = 0;
			SaveDataToSettings("AutoAssign", "Automatic assignment of squawk codes", "0");
			DisplayUserMessage(MY_PLUGIN_NAME, "Setting changed", "Automatic code assignment disabled", true, true, false, false, false);
		}
		else
		{
			autoAssign = 10;
			SaveDataToSettings("AutoAssign", "Automatic assignment of squawk codes", "10");
			DisplayUserMessage(MY_PLUGIN_NAME, "Setting changed", "Automatic code assignment enabled (default refresh rate 10 seconds)", true, true, false, false, false);
		}
		return true;
	}
	else if (sCommand == "tracking")
	{
		if (updateOnStartTracking)
		{
			updateOnStartTracking = false;
			SaveDataToSettings("updateOnStartTracking", "Validating squawk when starting to track an aircraft", "0");
			DisplayUserMessage(MY_PLUGIN_NAME, "Setting changed", "Validating squawk when starting to track an aircraft disabled", true, true, false, false, false);
		}
		else
		{
			updateOnStartTracking = true;
			SaveDataToSettings("updateOnStartTracking", "Validating squawk when starting to track an aircraft", "1");
			DisplayUserMessage(MY_PLUGIN_NAME, "Setting changed", "Validating squawk when starting to track an aircraft enabled", true, true, false, false, false);
		}
		return true;
	}
	else if (sCommand == "reload")
	{
		fVersion = async_http_get(MY_PLUGIN_UPDATE_BASE, MY_PLUGIN_UPDATE_ENDPOINT);
		fConfig = async_http_get(MY_PLUGIN_CONFIG_BASE, MY_PLUGIN_CONFIG_ENDPOINT);
		ReadSettings();
		return true;
	}
#ifdef _DEBUG
	else if (sCommand == "reset")
	{
		ProcessedFlightPlans.clear();
		return true;
	}
	else if (sCommand == "esver")
	{
		std::vector<int> version = GetExeVersion();
		if (version.empty()) {
			DisplayUserMessage(MY_PLUGIN_NAME, "Debug", "Failed to retrieve executable version.", true, false, false, false, false);
		}
		else {
			DisplayUserMessage(MY_PLUGIN_NAME, "Debug", string("Executable Version: " + to_string(version[0]) + "." + to_string(version[1]) + "." + to_string(version[2]) + "." + to_string(version[3])).c_str(), true, false, false, false, false);
		}
		return true;
	}
	else if (sCommand == "list")
	{
		string DisplayMsg;
		for (auto& pfp : ProcessedFlightPlans)
		{
			if (DisplayMsg.length() == 0)
				DisplayMsg = "Processed Flight Plans: " + pfp;
			else
				DisplayMsg += ", " + pfp;
		}
		
		DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
		return true;
	}
	else if (sCommand == "sqlist")
	{
		for (CRadarTarget RadarTarget = RadarTargetSelectFirst(); RadarTarget.IsValid();
			RadarTarget = RadarTargetSelectNext(RadarTarget))
		{
			string DisplayMsg = "Status " + string{ (RadarTarget.GetCorrelatedFlightPlan().GetSimulated() ? "simulated" : "not sim") } +
				", FP Type '" + RadarTarget.GetCorrelatedFlightPlan().GetFlightPlanData().GetPlanType() + "'" + 
				", AC info '" + RadarTarget.GetCorrelatedFlightPlan().GetFlightPlanData().GetAircraftInfo() + "' / '" + to_string(RadarTarget.GetCorrelatedFlightPlan().GetFlightPlanData().GetCapibilities()) + "'" +
				", " + to_string(RadarTarget.GetCorrelatedFlightPlan().GetSectorEntryMinutes()) + " Minutes to Sector Entry, " + 
				(HasValidSquawk(RadarTarget.GetCorrelatedFlightPlan()) ? "has valid squawk" : "has NO valid squawk") +
				", ASSIGNED '" + RadarTarget.GetCorrelatedFlightPlan().GetControllerAssignedData().GetSquawk() + "', SET " + RadarTarget.GetPosition().GetSquawk();
			DisplayUserMessage((MY_PLUGIN_NAME + (string)" Squawk List Dump").c_str(), RadarTarget.GetCallsign(), DisplayMsg.c_str(), true, false, false, false, false);
		}
		return true;
	}
	else
	{
		for (CFlightPlan FlightPlan = FlightPlanSelectFirst(); FlightPlan.IsValid();
			FlightPlan = FlightPlanSelectNext(FlightPlan))
		{
			if (_stricmp(sCommand.c_str(), FlightPlan.GetCallsign()) == 0)
			{
				if (Command[2].str() == "assign")
				{
					AssignAutoSquawk(FlightPlan);
					return true;
				}

				if (!ControllerMyself().IsValid() || !ControllerMyself().IsController() || (ControllerMyself().GetFacility() > 1 && ControllerMyself().GetFacility() < 5))
					DisplayUserMessage((MY_PLUGIN_NAME + (string)" FP Status").c_str(), "Debug", "This controller is not allowed to automatically assign squawks", true, false, false, false, false);

				string DisplayMsg = (FlightPlan.GetSimulated() ? "simulated" : "not sim") + (string)", FP received: " + (FlightPlan.GetFlightPlanData().IsReceived() ? "YES" : "NO") + ", FP Type '" + FlightPlan.GetFlightPlanData().GetPlanType() + "'" +
					", AC info '" + FlightPlan.GetFlightPlanData().GetAircraftInfo() + "' / '" + (FlightPlan.GetFlightPlanData().GetCapibilities() == '\n' ? "(EOL)" : string{ FlightPlan.GetFlightPlanData().GetCapibilities() }) + "'" +
					", " + to_string(FlightPlan.GetSectorEntryMinutes()) + " Minutes to Sector Entry, " + (HasValidSquawk(FlightPlan) ? "has valid squawk" : "has NO valid squawk") +
					", ASSIGNED '" + FlightPlan.GetControllerAssignedData().GetSquawk() + "', SET " + FlightPlan.GetCorrelatedRadarTarget().GetPosition().GetSquawk();
				DisplayUserMessage((MY_PLUGIN_NAME + (string)" FP Status").c_str(), FlightPlan.GetCallsign(), DisplayMsg.c_str(), true, false, false, false, false);

				if (find(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()) != ProcessedFlightPlans.end())
					DisplayUserMessage((MY_PLUGIN_NAME + (string)" FP Status").c_str(), FlightPlan.GetCallsign(), "This flight plan has already been processed", true, false, false, false, false);

				for (CRadarTarget RadarTarget = RadarTargetSelectFirst(); RadarTarget.IsValid();
					RadarTarget = RadarTargetSelectNext(RadarTarget))
				{
					if (_stricmp(RadarTarget.GetCallsign(), FlightPlan.GetCallsign()) == 0 || strlen(FlightPlan.GetControllerAssignedData().GetSquawk()) != 4 || _stricmp(FlightPlan.GetControllerAssignedData().GetSquawk(), squawkModeS) == 0 || _stricmp(FlightPlan.GetControllerAssignedData().GetSquawk(), squawkVFR) == 0)
						continue;
					else if (_stricmp(RadarTarget.GetCorrelatedFlightPlan().GetControllerAssignedData().GetSquawk(), FlightPlan.GetControllerAssignedData().GetSquawk()) == 0)
					{
						DisplayMsg = "also used for " + string{ RadarTarget.GetCallsign() } + ", " + (RadarTarget.GetCorrelatedFlightPlan().GetSimulated() ? "simulated" : "not sim") +
							", FP Type '" + RadarTarget.GetCorrelatedFlightPlan().GetFlightPlanData().GetPlanType() + "', " + to_string(RadarTarget.GetCorrelatedFlightPlan().GetSectorEntryMinutes()) +
							" Minutes to Sector Entry, " + (HasValidSquawk(RadarTarget.GetCorrelatedFlightPlan()) ? "has valid squawk" : "has NO valid squawk") + 
							", ASSIGNED '" + RadarTarget.GetCorrelatedFlightPlan().GetControllerAssignedData().GetSquawk() + "', SET " + RadarTarget.GetPosition().GetSquawk();
						DisplayUserMessage((MY_PLUGIN_NAME + (string)" FP Status").c_str(), "ASSR (RT)", DisplayMsg.c_str(), true, false, false, false, false);
					}
					else if (_stricmp(RadarTarget.GetPosition().GetSquawk(), FlightPlan.GetControllerAssignedData().GetSquawk()) == 0)
					{
						DisplayMsg = "also used for " + string{ RadarTarget.GetCallsign() } + ", " + (RadarTarget.GetCorrelatedFlightPlan().GetSimulated() ? "simulated" : "not sim") +
							", FP Type '" + RadarTarget.GetCorrelatedFlightPlan().GetFlightPlanData().GetPlanType() + "', " + to_string(RadarTarget.GetCorrelatedFlightPlan().GetSectorEntryMinutes()) +
							" Minutes to Sector Entry, " + (HasValidSquawk(RadarTarget.GetCorrelatedFlightPlan()) ? "has valid squawk" : "has NO valid squawk") + 
							", ASSIGNED '" + RadarTarget.GetCorrelatedFlightPlan().GetControllerAssignedData().GetSquawk() + "', SET " + RadarTarget.GetPosition().GetSquawk();
						DisplayUserMessage((MY_PLUGIN_NAME + (string)" FP Status").c_str(), "PSSR (RT)", DisplayMsg.c_str(), true, false, false, false, false);
					}
				}

				for (CFlightPlan FP = FlightPlanSelectFirst(); FP.IsValid(); FP = FlightPlanSelectNext(FP))
				{
					if (strcmp(FP.GetCallsign(), FlightPlan.GetCallsign()) == 0 || strlen(FlightPlan.GetControllerAssignedData().GetSquawk()) != 4 || _stricmp(FlightPlan.GetControllerAssignedData().GetSquawk(), squawkModeS) == 0 || _stricmp(FlightPlan.GetControllerAssignedData().GetSquawk(), squawkVFR) == 0)
						continue;

					if (strcmp(FlightPlan.GetControllerAssignedData().GetSquawk(), FP.GetControllerAssignedData().GetSquawk()) == 0)
					{
						DisplayMsg = "also used for " + string{ FP.GetCallsign() } + ", " + (FP.GetSimulated() ? "simulated" : "not sim") +
							", FP Type '" + FP.GetFlightPlanData().GetPlanType() + "', " + to_string(FP.GetSectorEntryMinutes()) +
							" Minutes to Sector Entry, " + (HasValidSquawk(FP) ? "has valid squawk" : "has NO valid squawk") +
							", ASSIGNED '" + FP.GetControllerAssignedData().GetSquawk() + "', SET " + FlightPlan.GetCorrelatedRadarTarget().GetPosition().GetSquawk();
						DisplayUserMessage((MY_PLUGIN_NAME + (string)" FP Status").c_str(), "ASSR (FP)", DisplayMsg.c_str(), true, false, false, false, false);
					}
				}
				return true;
			}
		}
	}
#endif
	return false;
}

void CCAMS::OnGetTagItem(CFlightPlan FlightPlan, CRadarTarget RadarTarget, int ItemCode, int TagData, char sItemString[16], int * pColorCode, COLORREF * pRGB, double * pFontSize)
{
	if (!FlightPlan.IsValid())
		return;

	if (ItemCode == ItemCodes::TAG_ITEM_ISMODES)
	{
		if (IsAcModeS(FlightPlan))
			strcpy_s(sItemString, 16, "S");
		else
			strcpy_s(sItemString, 16, "A");
	}
	else
	{
		if (!RadarTarget.IsValid())
			return;

		if (ItemCode == ItemCodes::TAG_ITEM_EHS_HDG)
		{
			if (tagColour != CLR_INVALID) 
			{
				*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;
				*pRGB = tagColour;
			}
			if (IsEHS(FlightPlan))
			{
				sprintf_s(sItemString, 16, "%03i°", RadarTarget.GetPosition().GetReportedHeading() % 360);
	#ifdef _DEBUG
				string DisplayMsg{ to_string(RadarTarget.GetPosition().GetReportedHeading()) };
				//DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
	#endif
			}
			else
			{
				strcpy_s(sItemString, 16, "N/A");
			}
		}
		else if (ItemCode == ItemCodes::TAG_ITEM_EHS_ROLL)
		{
			if (tagColour != CLR_INVALID)
			{
				*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;
				*pRGB = tagColour;
			}
			if (IsEHS(FlightPlan))
			{
				auto rollb = RadarTarget.GetPosition().GetReportedBank();

				if (rollb == 0)
				{
					sprintf_s(sItemString, 16, "%i", abs(rollb));
				}
				else
				{
					sprintf_s(sItemString, 16, "%c%i°", rollb > 0 ? 'R' : 'L', abs(rollb));
				}
	#ifdef _DEBUG
				string DisplayMsg{ to_string(abs(rollb)) };
				//DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
	#endif
			}
			else
			{
				strcpy_s(sItemString, 16, "N/A");
			}
		}
		else if (ItemCode == ItemCodes::TAG_ITEM_EHS_GS)
		{
			if (tagColour != CLR_INVALID)
			{
				*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;
				*pRGB = tagColour;
			}
			if (IsEHS(FlightPlan) && FlightPlan.GetCorrelatedRadarTarget().IsValid())
			{
				snprintf(sItemString, 16, "%03i", RadarTarget.GetPosition().GetReportedGS());
	#ifdef _DEBUG
				string DisplayMsg{ to_string(RadarTarget.GetPosition().GetReportedGS()) };
				//DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
	#endif
			}
			else
			{
				strcpy_s(sItemString, 16, "N/A");
			}
		}
		else if (ItemCode == ItemCodes::TAG_ITEM_ERROR_MODES_USE)
		{
			if (IsEligibleSquawkModeS(FlightPlan)) return;

			auto assr = RadarTarget.GetCorrelatedFlightPlan().GetControllerAssignedData().GetSquawk();
			auto pssr = RadarTarget.GetPosition().GetSquawk();
			if (strcmp(assr, squawkModeS) != 0 &&
				strcmp(pssr, squawkModeS) != 0)
				return;

			*pColorCode = EuroScopePlugIn::TAG_COLOR_INFORMATION;
			strcpy_s(sItemString, 16, "MSSQ");
		}
		else if (ItemCode == ItemCodes::TAG_ITEM_SQUAWK)
		{
			auto assr = RadarTarget.GetCorrelatedFlightPlan().GetControllerAssignedData().GetSquawk();
			auto pssr = RadarTarget.GetPosition().GetSquawk();

			if (strcmp(pssr, "7500") == 0 || strcmp(pssr, "7600") == 0 || strcmp(pssr, "7700") == 0)
			{
				*pColorCode = EuroScopePlugIn::TAG_COLOR_EMERGENCY;
			}
			else if ((!IsEligibleSquawkModeS(FlightPlan) && (strcmp(assr, squawkModeS) == 0 || (strcmp(pssr, squawkModeS) == 0 && strlen(assr) == 0))) || HasDuplicateSquawk(FlightPlan))
			{
				// mode S code assigned, but not eligible
				// or duplicate is detected
				*pColorCode = EuroScopePlugIn::TAG_COLOR_REDUNDANT;
			}
			else if (strcmp(assr, pssr) != 0)
			{
				// assigned squawk is not set
				*pColorCode = EuroScopePlugIn::TAG_COLOR_INFORMATION;
			}
			strcpy_s(sItemString, 16, assr);
		}
		else if (ItemCode == ItemCodes::TAG_ITEM_EHS_PINNED)
		{
			if (tagColour != CLR_INVALID)
			{
				*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;
				*pRGB = tagColour;
			}
			if (std::find(EHSListFlightPlans.begin(), EHSListFlightPlans.end(), FlightPlan.GetCallsign()) != EHSListFlightPlans.end())
				strcpy_s(sItemString, 16, "¤");
			else
				strcpy_s(sItemString, 16, "¬");
		}
	}
}

void CCAMS::OnFlightPlanDisconnect(CFlightPlan FlightPlan)
{
	ProcessedFlightPlans.erase(remove(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()), ProcessedFlightPlans.end());
	EHSListFlightPlans.erase(remove(EHSListFlightPlans.begin(), EHSListFlightPlans.end(), FlightPlan.GetCallsign()), EHSListFlightPlans.end());

	FpListEHS.RemoveFpFromTheList(FlightPlan);
}

void CCAMS::OnFlightPlanFlightPlanDataUpdate(CFlightPlan FlightPlan)
{
	string DisplayMsg;
#ifdef _DEBUG
	stringstream log;
#endif
	if (!HasValidSquawk(FlightPlan))
	{
		if (std::find(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()) != ProcessedFlightPlans.end() && updateOnStartTracking)
		{
			ProcessedFlightPlans.erase(remove(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()), ProcessedFlightPlans.end());
#ifdef _DEBUG
			log << FlightPlan.GetCallsign() << ":FP removed from processed list:no valid squawk assigned";
			writeLogFile(log);
			DisplayMsg = string{ FlightPlan.GetCallsign() } + " removed from processed list because it has no valid squawk assigned";
			DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
		}
	}
//	else if (FlightPlan.GetTrackingControllerIsMe())
//	{
//		if (autoAssign > 0 && pluginVersionCheck && ConnectionState > 10)
//		{
//#ifdef _DEBUG
//			log << FlightPlan.GetCallsign() << ":FP processed for automatic squawk assignment:flight plan update and controller is tracking";
//			writeLogFile(log);
//			string DisplayMsg = string{ FlightPlan.GetCallsign() } + " is processed for automatic squawk assignment (due to flight plan update and controller is tracking)";
//			DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, true, false, false, false);
//#endif
//			if (FlightPlan.GetCorrelatedRadarTarget().IsValid())
//				AssignAutoSquawk(FlightPlan);
//		}
//	}
}

void CCAMS::OnFlightPlanFlightStripPushed(CFlightPlan FlightPlan, const char* sSenderController, const char* sTargetController)
{
#ifdef _DEBUG
	stringstream log;
#endif
	if (strcmp(sTargetController, FlightPlan.GetCallsign()) == 0)
	{
		// shouldn't be required that often anymore (if at all) since call signs are not added to the list that generously
		auto it = find(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign());
		if (it != ProcessedFlightPlans.end()) {
			ProcessedFlightPlans.erase(remove(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()), ProcessedFlightPlans.end());
#ifdef _DEBUG
			log << FlightPlan.GetCallsign() << ":FP removed from processed list:strip push received";
			writeLogFile(log);
			string DisplayMsg = string{ FlightPlan.GetCallsign() } + " removed from processed list because a strip push has been received";
			DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
		}
	}

}

void CCAMS::OnRefreshFpListContent(CFlightPlanList AcList)
{

	if (ControllerMyself().IsValid() && RadarTargetSelectASEL().IsValid())
	{
#ifdef _DEBUG
		string DisplayMsg{ "The following call sign was identified to be added to the EHS Mode S list: " + string { FlightPlanSelectASEL().GetCallsign() } };
		//DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
		for (CFlightPlan FP = FlightPlanSelectFirst(); FP.IsValid(); FP = FlightPlanSelectNext(FP))
		{
			if (std::find(EHSListFlightPlans.begin(), EHSListFlightPlans.end(), FP.GetCallsign()) == EHSListFlightPlans.end())
				FpListEHS.RemoveFpFromTheList(FP);
		}
		FpListEHS.AddFpToTheList(FlightPlanSelectASEL());

	}
}

void CCAMS::OnFunctionCall(int FunctionId, const char* sItemString, POINT Pt, RECT Area)
{
	CFlightPlan FlightPlan = FlightPlanSelectASEL();

	if (FunctionId == ItemCodes::TAG_FUNC_TOGGLE_EHS_LIST)
	{
		if (std::find(EHSListFlightPlans.begin(), EHSListFlightPlans.end(), FlightPlan.GetCallsign()) != EHSListFlightPlans.end())
			EHSListFlightPlans.erase(remove(EHSListFlightPlans.begin(), EHSListFlightPlans.end(), FlightPlan.GetCallsign()), EHSListFlightPlans.end());
		else
			EHSListFlightPlans.push_back(FlightPlan.GetCallsign());
		FpListEHS.AddFpToTheList(FlightPlan);
		return;
	}

	if (!ControllerMyself().IsValid() || !ControllerMyself().IsController())
		return;

	if (!FlightPlan.IsValid() || FlightPlan.GetSimulated())
		return;

	if (!FlightPlan.GetTrackingControllerIsMe() && strlen(FlightPlan.GetTrackingControllerCallsign())>0)
		return;

	switch (FunctionId)
	{
	case ItemCodes::TAG_FUNC_SQUAWK_POPUP:
		OpenPopupList(Area, "Squawk", 1);
		AddPopupListElement("Auto assign", "", ItemCodes::TAG_FUNC_ASSIGN_SQUAWK_AUTO);
		AddPopupListElement("Manual set", "", ItemCodes::TAG_FUNC_ASSIGN_SQUAWK_MANUAL);
		AddPopupListElement("Discrete", "", ItemCodes::TAG_FUNC_ASSIGN_SQUAWK_DISCRETE);
		AddPopupListElement("VFR", "", ItemCodes::TAG_FUNC_ASSIGN_SQUAWK_VFR);
		break;
	case ItemCodes::TAG_FUNC_ASSIGN_SQUAWK_MANUAL:
		OpenPopupEdit(Area, ItemCodes::TAG_FUNC_ASSIGN_SQUAWK, "");
		break;
	case ItemCodes::TAG_FUNC_ASSIGN_SQUAWK:
		FlightPlan.GetControllerAssignedData().SetSquawk(sItemString);
		
		if (find(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()) == ProcessedFlightPlans.end())
			ProcessedFlightPlans.push_back(FlightPlan.GetCallsign());

		break;
	case ItemCodes::TAG_FUNC_ASSIGN_SQUAWK_AUTO:
		if (IsEligibleSquawkModeS(FlightPlan))
		{
			FlightPlan.GetControllerAssignedData().SetSquawk(squawkModeS);
			return;
		}
		// continue with discrete assignment if Mode S squawk is not applicable
	case ItemCodes::TAG_FUNC_ASSIGN_SQUAWK_DISCRETE:
		try
		{
			if (find(PendingSquawkRequests.begin(), PendingSquawkRequests.end(), FlightPlan.GetCallsign()) == PendingSquawkRequests.end())
			{
				PendingSquawkRequests.push_back(FlightPlan.GetCallsign());
#ifdef _DEBUG
				if (GetConnectionType() == 4)
				{
					string DisplayMsg{ "A request for a replay session has been detected: " + string { FlightPlan.GetCallsign() } };
					DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
				}
				else if (GetConnectionType() > 2)
				{
					string DisplayMsg{ "A request for a simulated aircraft has been detected: " + string { FlightPlan.GetCallsign() } };
					DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
				}
#endif
			}
		}
		catch (std::runtime_error const& e)
		{
			DisplayUserMessage(MY_PLUGIN_NAME, "Error", e.what(), true, true, false, false, false);
		}
		catch (...)
		{
			DisplayUserMessage(MY_PLUGIN_NAME, "Error", std::to_string(GetLastError()).c_str(), true, true, false, false, false);
		}
		break;
	case ItemCodes::TAG_FUNC_ASSIGN_SQUAWK_VFR:
		FlightPlan.GetControllerAssignedData().SetSquawk(squawkVFR);
		break;
	default:
		break;
	}
}

void CCAMS::OnTimer(int Counter)
{
#ifdef _DEBUG
	stringstream log;
#endif
	if (fVersion.valid() && fVersion.wait_for(chrono::milliseconds(0)) == std::future_status::ready)
		CheckVersion(fVersion);

	if (fConfig.valid() && fConfig.wait_for(chrono::milliseconds(0)) == std::future_status::ready)
		LoadConfig(fConfig);

	if (ControllerMyself().IsValid() && ControllerMyself().IsController() && GetConnectionType() > 0)
		if (GetConnectionType() != 4 || ConnectionState != 4)
		{
			ConnectionState++;
			RemoteConnectionState++;
		}
	else if (GetConnectionType() != ConnectionState)
	{
		ConnectionState = 0;
		RemoteConnectionState = 0;
		if (ProcessedFlightPlans.size() > 0)
		{
			ProcessedFlightPlans.clear();
#ifdef _DEBUG
			DisplayUserMessage(MY_PLUGIN_NAME, "Debug", "Connection Status 0 detected, all processed flight plans are removed from the list", true, false, false, false, false);
#endif
		}
	}

#ifdef _DEBUG
	if (ConnectionState == 10)
		DisplayUserMessage(MY_PLUGIN_NAME, "Debug", "Active connection established, automatic squawk assignment enabled", true, false, false, false, false);
#endif

#ifdef _DEBUG
	if (ConnectionState > 10 || GetConnectionType() == 4)
#else
	if (ConnectionState > 10)
#endif // _DEBUG
	{
		AssignPendingSquawks();
		if (GetConnectionType() <= 2 || Counter % 2 == 0) RequestSquawks();

		if (autoAssign == 0 || !pluginVersionCheck || ControllerMyself().GetRating() < 2 || (ControllerMyself().GetFacility() > 1 && ControllerMyself().GetFacility() < 5))
			return;
		else if (!(Counter % autoAssign))
		{
#ifdef _DEBUG
			log << "AutoAssignTimer:Starting automatic squawk assignments";
			writeLogFile(log);
			DisplayUserMessage(MY_PLUGIN_NAME, "Debug", "Starting timer-based automatic squawk assignments", true, false, false, false, false);
#endif // _DEBUG

			for (CRadarTarget RadarTarget = RadarTargetSelectFirst(); RadarTarget.IsValid();
				RadarTarget = RadarTargetSelectNext(RadarTarget))
			{
				AssignAutoSquawk(RadarTarget.GetCorrelatedFlightPlan());
			}
		}
	}
}

void CCAMS::AssignAutoSquawk(CFlightPlan& FlightPlan)
{
	string DisplayMsg;
#ifdef _DEBUG
	stringstream log;
#endif
	const char* assr = FlightPlan.GetControllerAssignedData().GetSquawk();
	const char* pssr = FlightPlan.GetCorrelatedRadarTarget().GetPosition().GetSquawk();

	// check controller class validity and qualification, restrict to APP/CTR/FSS controller types and respect a minimum connection duration (time)
	//if (!ControllerMyself().IsValid() || !ControllerMyself().IsController())
	//	return;

	if (FlightPlan.GetSimulated() || strcmp(FlightPlan.GetFlightPlanData().GetPlanType(), "V") == 0)
	{
		// disregard simulated flight plans (out of the controllers range)
		// disregard flight with flight rule VFR
		if (find(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()) == ProcessedFlightPlans.end())
		{
			ProcessedFlightPlans.push_back(FlightPlan.GetCallsign());
#ifdef _DEBUG
			log << FlightPlan.GetCallsign() << ":FP processed:Simulated/FP Type";
			writeLogFile(log);
			DisplayMsg = string{ FlightPlan.GetCallsign() } + " processed due to Simulation Flag / Flight Plan Type";
			DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
		}
		return;
	}
	else if (FlightPlan.GetFlightPlanData().IsReceived() && FlightPlan.GetSectorEntryMinutes() < 0)
	{
		// the flight will never enter the sector of the current controller
		if (find(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()) == ProcessedFlightPlans.end())
		{
			ProcessedFlightPlans.push_back(FlightPlan.GetCallsign());
#ifdef _DEBUG
			log << FlightPlan.GetCallsign() << ":FP processed:Sector Entry Time:" << FlightPlan.GetSectorEntryMinutes();
			writeLogFile(log);
			DisplayMsg = string{ FlightPlan.GetCallsign() } + " processed because it will not enter the controllers sector";
			DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
		}
		return;
	}
	else if (HasValidSquawk(FlightPlan))
	{
		// this flight has already assigned a valid code
		if (FlightPlan.GetTrackingControllerIsMe())
		{
			if (find(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()) == ProcessedFlightPlans.end())
			{
				ProcessedFlightPlans.push_back(FlightPlan.GetCallsign());
#ifdef _DEBUG
				log << FlightPlan.GetCallsign() << ":FP processed:has already a valid squawk:" << assr << ":" << pssr;
				writeLogFile(log);
				DisplayMsg = string{ FlightPlan.GetCallsign() } + " processed because it has already a valid squawk (ASSIGNED '" + assr + "', SET " + pssr + ")";
				DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
			}
		}
		// if this flight is not tracked by the current controller yet, it is kept for revalidation in the next round

		return;
	}
	//else if (HasDuplicateSquawk(FlightPlan.GetCorrelatedRadarTarget()))
	else if (find(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()) != ProcessedFlightPlans.end())
	{
		// The flight was already processed, but the assigned code has become invalid again
		// This is probably due to a duplicate, where the code assigned earlier was assigned to a second aircraft by another controller
		if (FlightPlan.GetTrackingControllerIsMe())
		//if (FlightPlan.GetTrackingControllerIsMe() && !HasDuplicatePSSR(FlightPlan))
		{
			// attempting to change to squawk of the other aircraft
			for (CFlightPlan FP = FlightPlanSelectFirst(); FP.IsValid(); FP = FlightPlanSelectNext(FP))
			{
				if (_stricmp(FP.GetCallsign(), FlightPlan.GetCallsign()) == 0)
					continue;
				else if (_stricmp(FP.GetControllerAssignedData().GetSquawk(), FlightPlan.GetControllerAssignedData().GetSquawk()) == 0)
				{
					if (FP.GetTrackingControllerIsMe())
						break;
					else if (strlen(FP.GetTrackingControllerCallsign()) > 0)
						break; // this will require a new code for the processed flight
					else if (strlen(FP.GetFlightPlanData().GetOrigin()) < 4 || strlen(FP.GetFlightPlanData().GetDestination()) < 4 || FP.GetCorrelatedRadarTarget().GetGS() < APTcodeMaxGS)
						break;
					else if (IsADEPvicinity(FP) || FP.GetDistanceToDestination() < APTcodeMaxDist)
						break;
					else if (FP.GetCorrelatedRadarTarget().IsValid())
						if (_stricmp(FP.GetCorrelatedRadarTarget().GetPosition().GetSquawk(), FlightPlan.GetControllerAssignedData().GetSquawk()) == 0)
							break;

					if (find(PendingSquawkRequests.begin(), PendingSquawkRequests.end(), FP.GetCallsign()) == PendingSquawkRequests.end())
						PendingSquawkRequests.push_back(FP.GetCallsign());
#ifdef _DEBUG
					log << FP.GetCallsign() << ":duplicate assigned code:unique code AUTO assigned:" << FlightPlan.GetCallsign() << " already tracked by " << FlightPlan.GetTrackingControllerCallsign();
					writeLogFile(log);
					DisplayMsg = string{ FP.GetCallsign() } + ", unique code AUTO assigned due to a detected duplicate with " + FlightPlan.GetCallsign();
					DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
				}
			}
			return;
		}
		else
		{
			// removing the call sign from the processed flight plan list to initiate a new assignment
			ProcessedFlightPlans.erase(remove(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()), ProcessedFlightPlans.end());
#ifdef _DEBUG
			log << FlightPlan.GetCallsign() << ":duplicate assigned code:FP removed from processed list";
			writeLogFile(log);
			string DisplayMsg = string{ FlightPlan.GetCallsign() } + " removed from processed list because the assigned code is no longer valid";
			DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
		}
	}
	else
	{
#ifdef _DEBUG
		DisplayMsg = string{ FlightPlan.GetCallsign() } + " has NOT a valid squawk code (ASSIGNED '" + assr + "', SET " + pssr + "), continue checks if eligible for automatic squawk assignment";
		//DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
	}

	// disregard if no flight plan received (= no ADES/ADEP), or low speed (considered not flying yet)
	if (strlen(FlightPlan.GetFlightPlanData().GetOrigin()) < 4 || strlen(FlightPlan.GetFlightPlanData().GetDestination()) < 4 || FlightPlan.GetCorrelatedRadarTarget().GetGS() < APTcodeMaxGS)
		return;

	// disregard if the flight is assumed in the vicinity of the departure or arrival airport
	if (IsADEPvicinity(FlightPlan) || FlightPlan.GetDistanceToDestination() < APTcodeMaxDist)
		return;

#ifdef _DEBUG
	DisplayMsg = string{ FlightPlan.GetCallsign() } + ": Tracking Controller Len '" + to_string(strlen(FlightPlan.GetTrackingControllerCallsign())) + "', CoordNextC '" + string{ FlightPlan.GetCoordinatedNextController() } + "', Minutes to entry " + to_string(FlightPlan.GetSectorEntryMinutes()) + ", TrackingMe: " + to_string(FlightPlan.GetTrackingControllerIsMe());
	//DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
	if (!FlightPlan.GetTrackingControllerIsMe())
	{
		// the current controller is not tracking the flight plan
		CFlightPlanPositionPredictions Pos = FlightPlan.GetPositionPredictions();
		int min;

		for (min = 0; min < Pos.GetPointsNumber(); min++)
		{
			if (min <= 15 && _stricmp(FlightPlan.GetPositionPredictions().GetControllerId(min), "--") != 0)
			{
				break;
			}
		}

		if (strlen(FlightPlan.GetTrackingControllerCallsign()) > 0)
		{
			// another controller is currently tracking the flight
			return;
		}
		else if (_stricmp(ControllerMyself().GetPositionId(), FlightPlan.GetPositionPredictions().GetControllerId(min)) != 0)
		{
			// the current controller is not the next controller of this flight
			return;
		}
		else if (FlightPlan.GetSectorEntryMinutes() > 15)
		{
			// the flight is still too far away from the current controllers sector
			return;
		}
		else
		{
#ifdef _DEBUG
			// The current controller is not tracking the flight, but automatic squawk assignment is applicable
			DisplayMsg = string{ FlightPlan.GetCallsign() } + " IS eligible for automatic squawk assignment. ASSIGNED '" + assr + "', SET " + pssr + ", Sector entry in " + to_string(FlightPlan.GetSectorEntryMinutes()) + " MIN";
			DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
		}
	}

	// if the function has not been ended, the flight is subject to automatic squawk assignment
#ifdef _DEBUG
	DisplayMsg = string{ FlightPlan.GetCallsign() } + ", AC info '" + FlightPlan.GetFlightPlanData().GetAircraftInfo() + "' / '" + to_string(FlightPlan.GetFlightPlanData().GetCapibilities()) + "'";
	DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
	if (IsEligibleSquawkModeS(FlightPlan))
	{
		FlightPlan.GetControllerAssignedData().SetSquawk(squawkModeS);
#ifdef _DEBUG
		log << FlightPlan.GetCallsign() << ":FP processed:Mode S code AUTO assigned";
		writeLogFile(log);
		DisplayMsg = string{ FlightPlan.GetCallsign() } + ", code 1000 AUTO assigned";
		DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
	}
	else if (find(PendingSquawkRequests.begin(), PendingSquawkRequests.end(), FlightPlan.GetCallsign()) == PendingSquawkRequests.end())
	{
		PendingSquawkRequests.push_back(FlightPlan.GetCallsign());
#ifdef _DEBUG
		log << FlightPlan.GetCallsign() << ":FP processed:unique code AUTO assigned";
		writeLogFile(log);
		DisplayMsg = string{ FlightPlan.GetCallsign() } + ", unique code AUTO assigned";
		DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
	}

	ProcessedFlightPlans.push_back(FlightPlan.GetCallsign());
}

void CCAMS::AssignSquawk(CFlightPlan& FlightPlan)
{
	if (find(PendingSquawkRequests.begin(), PendingSquawkRequests.end(), FlightPlan.GetCallsign()) == PendingSquawkRequests.end())
		PendingSquawkRequests.push_back(FlightPlan.GetCallsign());

	//future<string> webSquawk = std::async(LoadWebSquawk, FlightPlan, ControllerMyself(), collectUsedCodes(FlightPlan), IsADEPvicinity(FlightPlan), GetConnectionType());

	//if (webSquawk.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
	//{
	//	string squawk = webSquawk.get();
	//	if (!FlightPlanSelect(FlightPlan.GetCallsign()).GetControllerAssignedData().SetSquawk(squawk.c_str()))
	//	{
	//		PendingSquawkRequests.push_back(FlightPlan.GetCallsign());
	//	}
	//}
}

void CCAMS::AssignPendingSquawks()
{
	string DisplayMsg;

	for (auto it = PendingSquawks.begin(); it != PendingSquawks.end(); )
	{
		if (it->second.valid() && it->second.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
		{
			std::string squawk = it->second.get();

			if (!FlightPlanSelect(it->first).IsValid())
			{
#ifdef _DEBUG
				DisplayMsg = string{ it->first } + " flight plan no longer valid, removing from pending squawks";
				DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
			}
			else if (!FlightPlanSelect(it->first).GetControllerAssignedData().SetSquawk(squawk.c_str()))
			{
				DisplayMsg = { "Your request for a squawk from the centralised code server failed. Check your plugin version, try again or revert to the ES built-in functionalities for assigning a squawk (F9)." };
				DisplayUserMessage(MY_PLUGIN_NAME, "Error", DisplayMsg.c_str(), true, true, false, false, false);
				DisplayUserMessage(MY_PLUGIN_NAME, "Error", ("For troubleshooting, report error code '" + squawk + "'").c_str(), true, true, false, false, false);

				RemoteConnectionState = -10;
				DisplayMsg = { "Request for squawks from the centralised code server are temporarily paused. Operations will resume automatically in 20 seconds." };
				DisplayUserMessage(MY_PLUGIN_NAME, "Error", DisplayMsg.c_str(), true, true, false, false, false);
			}
			else
			{
#ifdef _DEBUG
				DisplayMsg = string{ it->first } + ", code " + squawk + " assigned";
				DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
			}
			it = PendingSquawks.erase(it);
		}
		else
		{
			it++;
		}
	}
}

void CCAMS::RequestSquawks()
{
	string DisplayMsg;
	CFlightPlan FlightPlan;

	if (RemoteConnectionState < 10)
		return;
	if (GetConnectionType() > 2 && !PendingSquawks.empty())
		return;	// ensure the last answer has been reveived when using simulator sessions only, because the received squawk needs to be assigned to a flight plan first, so that it is part of the used codes for the next request. For live requests, this is not necessary as the code will be reserved on the server.

	for (auto sCallsign = PendingSquawkRequests.begin(); sCallsign != PendingSquawkRequests.end(); )
	{
		DisplayMsg = { "Pending squawk for " + (string)*sCallsign };
		//DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);

		FlightPlan = FlightPlanSelect(*sCallsign);
		//PendingSquawkRequests.erase(remove(PendingSquawkRequests.begin(), PendingSquawkRequests.end(), sCallsign), PendingSquawkRequests.end());
		if (FlightPlan.IsValid())
		{
#ifdef _DEBUG
			DisplayMsg = { "Initiating squawk request for " + (string)*sCallsign };
			DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
			PendingSquawks.insert(std::make_pair(*sCallsign, std::async(LoadWebSquawk, FlightPlan, ControllerMyself(), collectUsedCodes(FlightPlan), IsADEPvicinity(FlightPlan), GetConnectionType())));
			sCallsign = PendingSquawkRequests.erase(sCallsign);

			break;	// exit function after one successful assignment to avoid an overload of requests being sent out simultaneously
		}
		else
		{
			sCallsign = PendingSquawkRequests.erase(sCallsign);
		}
	}
}

void CCAMS::CheckVersion(future<string> & fmessage)
{
	try
	{
		std::string content = fmessage.get();
		if (content.empty()) {
			throw error{ string { MY_PLUGIN_NAME } + " plugin couldn't access the online version information. Automatic code assignment therefore not available." };
		}
		else
		{
			std::istringstream stream(content);
			std::string line;
			std::vector<std::string> lines;
			int line_number = 1;
			while (std::getline(stream, line)) {
#ifdef _DEBUG
				string DisplayMsg = "Version information downloaded, Line " + to_string(line_number) + ": " + line;
				DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
				lines.push_back(line);
				line_number++;
			}

			std::vector<int> EuroScopeVersion = GetExeVersion();
			string versionPluginLatest = lines[0];
			string versionPluginMinimum = lines[1];


			if (EuroScopeVersion[0] == 3)
			{
				if (EuroScopeVersion[1] < 2 || EuroScopeVersion[2] < 2)
					DisplayUserMessage(MY_PLUGIN_NAME, "Compatibility Check", "Your version of EuroScope is not supported due to authentification requirements. Please visit forum.vatsim.net/t/euroscope-mandatory-update-authentication-changes/5643 for more information.", true, true, false, true, false);
				else if (compareVersions({ VER_FILEVERSION }, parseVersion(versionPluginMinimum)) < 0)
				{
					DisplayUserMessage(MY_PLUGIN_NAME, "Compatibility Check", "Your plugin version is outdated and the automatic code assignment therefore not available. Please change to the latest version. Visit github.com/kusterjs/CCAMS/releases", true, true, false, true, false);
					throw error{ "Your " + string { MY_PLUGIN_NAME } + " plugin (version " + VER_FILEVERSION_STR + ") is outdated and the automatic code assignment therefore not available. Please change to the latest version.\n\nVisit github.com/kusterjs/CCAMS/releases" };
				}
				else
				{
					if (EuroScopeVersion[2] > 3)
						DisplayUserMessage(MY_PLUGIN_NAME, "Compatibility Check", "Your version of EuroScope may provide unreliable aircraft equipment code information. Deactivate the automatic code assignment if Mode S equipped aircraft are not detected correctly.", true, true, false, true, false);
					if (EuroScopeVersion[2] == 10)
						DisplayUserMessage(MY_PLUGIN_NAME, "Compatibility Check", "Your version of EuroScope does not allow plug-ins to add/manage custom flight plan lists. The Mode S EHS list is therefore not available.", true, true, false, true, false);

					pluginVersionCheck = true;
				}
			}

			if (compareVersions({ VER_FILEVERSION }, parseVersion(versionPluginLatest)) < 0)
			{
				DisplayUserMessage(MY_PLUGIN_NAME, "Update", ("CCAMS plugin version " + (string)versionPluginLatest + " is now available. Please visit github.com/kusterjs/CCAMS/releases and download the latest version.").c_str(), true, true, false, true, false);
			}

		}
	}
	catch (modesexception & e)
	{
		e.whatMessageBox();
	}
	catch (exception & e)
	{
		MessageBox(NULL, e.what(), MY_PLUGIN_NAME, MB_OK | MB_ICONERROR);
	}
	fmessage = future<string>();
}

void CCAMS::LoadConfig(future<string>& fmessage)
{
	try
	{
		std::string content = fmessage.get();
		if (content.empty()) {
			throw error{ string { MY_PLUGIN_NAME } + " plugin couldn't access the online configuration information. Fallback to hardcoded values." };
		}
		else
		{
			std::istringstream stream(content);
			std::string line;
			std::vector<std::string> lines;
			int line_number = 1;
			while (std::getline(stream, line)) {
#ifdef _DEBUG
				string DisplayMsg = "Version information downloaded, Line " + to_string(line_number) + ": " + line;
				DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
				lines.push_back(line);
				switch (line_number) {
					case 1:
						// Match airport
						ModeSAirports = regex("^" + line, regex::icase);
						break;
					case 2:
						// NOT match airport
						ModeSAirportsExcl = regex("^" + line, regex::icase);
						break;
					case 3:
						// Match route
						ModeSRoute = regex(line, regex::icase);
						break;
					case 4:
						// NOT match route
						ModeSRouteExcl = regex(line, regex::icase);
						break;
					default:

						break;
				}

				line_number++;
			}
		}
	}
	catch (modesexception& e)
	{
		e.whatMessageBox();
	}
	catch (exception& e)
	{
		MessageBox(NULL, e.what(), MY_PLUGIN_NAME, MB_OK | MB_ICONERROR);
	}
	fmessage = future<string>();
}

void CCAMS::ReadSettings()
{
	// Overwrite setting values by plugin settings, if available
	try
	{
		const char* cstrSetting = GetDataFromSettings("codeVFR");
		if (cstrSetting != NULL)
		{
			if (regex_match(cstrSetting, std::regex("[0-7]{4}")))
			{
				squawkVFR = cstrSetting;
			}
		}

		cstrSetting = GetDataFromSettings("acceptFPLformatICAO");
		if (cstrSetting != NULL)
		{
			if (strcmp(cstrSetting, "0") == 0)
			{
				acceptEquipmentICAO = false;
			}
		}

		cstrSetting = GetDataFromSettings("acceptFPLformatFAA");
		if (cstrSetting != NULL)
		{
			if (strcmp(cstrSetting, "0") == 0)
			{
				acceptEquipmentFAA = false;
			}
		}

		cstrSetting = GetDataFromSettings("updateOnStartTracking");
		if (cstrSetting != NULL)
		{
			if (strcmp(cstrSetting, "0") == 0)
			{
				updateOnStartTracking = false;
			}
		}

		cstrSetting = GetDataFromSettings("AutoAssign");
		if (cstrSetting != NULL)
		{
			if (strcmp(cstrSetting, "0") == 0)
			{
				autoAssign = 0;
			}
			else if (stoi(cstrSetting) > 0)
			{
				autoAssign = stoi(cstrSetting);
			}
		}

		cstrSetting = GetDataFromSettings("tagColour");
		if (cstrSetting != NULL)
		{
			const char* hex = (cstrSetting[0] == '#') ? cstrSetting + 1 : cstrSetting;

			if (std::strlen(hex) != 6)
				DisplayUserMessage(MY_PLUGIN_NAME, "Plugin Settings Error", "The setting 'tagColour' is not of the expected length of 6 characters.", true, true, true, true, false);
			else
			{
				int r, g, b;
				std::stringstream ss;

				ss << std::hex << std::string(hex, 2);
				if (!(ss >> r)) DisplayUserMessage(MY_PLUGIN_NAME, "Plugin Settings Error", "The setting 'tagColour' has invalid characters (red).", true, true, true, true, false);
				else
				{
					ss.clear(); ss.str(std::string(hex + 2, 2));
					if (!(ss >> g)) DisplayUserMessage(MY_PLUGIN_NAME, "Plugin Settings Error", "The setting 'tagColour' has invalid characters (green).", true, true, true, true, false);
					else
					{
						ss.clear(); ss.str(std::string(hex + 4, 2));
						if (!(ss >> b)) DisplayUserMessage(MY_PLUGIN_NAME, "Plugin Settings Error", "The setting 'tagColour' has invalid characters (blue).", true, true, true, true, false);
						else 
						{
							tagColour = RGB(r, g, b);
							if (!(ss >> b)) DisplayUserMessage(MY_PLUGIN_NAME, "Initialisation", ("Tag colour RGB " + to_string(GetRValue(tagColour)) + ", " + to_string(GetGValue(tagColour)) + ", " + to_string(GetBValue(tagColour))).c_str(), true, true, true, true, false);
						}
					}
				}
			}
		}
	}
	catch (std::runtime_error const& e)
	{
		DisplayUserMessage(MY_PLUGIN_NAME, "Plugin Error", (string("Error: ") + e.what()).c_str(), true, true, true, true, true);
	}
	catch (...)
	{
		DisplayUserMessage(MY_PLUGIN_NAME, "Plugin Error", ("Unexpected error: " + std::to_string(GetLastError())).c_str(), true, true, true, true, true);
	}
}

inline bool CCAMS::IsFlightPlanProcessed(CFlightPlan& FlightPlan)
{
	string callsign { FlightPlan.GetCallsign() };
	for (auto &pfp : ProcessedFlightPlans)
		if (pfp.compare(callsign) == 0)
			return true;

	return false;
}

bool CCAMS::IsAcModeS(const CFlightPlan& FlightPlan) const
{
	return HasEquipment(FlightPlan, acceptEquipmentFAA, acceptEquipmentICAO, EquipmentCodesICAO);
}

double CCAMS::GetDistanceFromOrigin(const CFlightPlan& FlightPlan) const
{
	if (FlightPlan.GetExtractedRoute().GetPointsNumber() > 1)
		return FlightPlan.GetFPTrackPosition().GetPosition().DistanceTo(FlightPlan.GetExtractedRoute().GetPointPosition(0));

	for (EuroScopePlugIn::CSectorElement SectorElement = SectorFileElementSelectFirst(SECTOR_ELEMENT_AIRPORT); SectorElement.IsValid();
		SectorElement = SectorFileElementSelectNext(SectorElement, SECTOR_ELEMENT_AIRPORT))
	{
		if (strncmp(SectorElement.GetName(), FlightPlan.GetFlightPlanData().GetOrigin(), 4) == 0)
		{
			CPosition AirportPosition;
			if (SectorElement.GetPosition(&AirportPosition, 0))
				return FlightPlan.GetFPTrackPosition().GetPosition().DistanceTo(AirportPosition);

			break;
		}
	}
	return 0;
}

bool CCAMS::IsADEPvicinity(const CFlightPlan& FlightPlan) const
{
	if (FlightPlan.GetCorrelatedRadarTarget().GetGS() < APTcodeMaxGS &&
		GetDistanceFromOrigin(FlightPlan) < APTcodeMaxDist)
		return true;

	return false;
}

bool CCAMS::IsApModeS(const string& icao) const
{
	if (regex_search(icao, ModeSAirports) && !regex_search(icao, ModeSAirportsExcl))
		return true;

	return false;
}

bool CCAMS::IsRteModeS(const CFlightPlan& FlightPlan) const
{
	CFlightPlanExtractedRoute FlightPlanExtractedRoute = FlightPlan.GetExtractedRoute();
	for (size_t i = 0; i < FlightPlanExtractedRoute.GetPointsNumber(); i++)
	{
		if (regex_search(FlightPlanExtractedRoute.GetPointName(i), ModeSRouteExcl))
			return false;
	}

	if (regex_search(FlightPlan.GetFlightPlanData().GetRoute(), ModeSRoute) && !regex_search(FlightPlan.GetFlightPlanData().GetRoute(), ModeSRouteExcl))
		return true;

	for (size_t i = 0; i < FlightPlanExtractedRoute.GetPointsNumber(); i++)
	{
		if (regex_search(FlightPlanExtractedRoute.GetPointName(i), ModeSRoute))
			return true;
	}

	return false;
}

bool CCAMS::IsEHS(const CFlightPlan& FlightPlan) const
{
	return HasEquipment(FlightPlan, acceptEquipmentFAA, true, EquipmentCodesICAOEHS);
}

bool CCAMS::HasEquipment(const CFlightPlan& FlightPlan, bool acceptEquipmentFAA, bool acceptEquipmentICAO, string CodesICAO) const
{
	//check for ICAO suffix
	if (acceptEquipmentICAO)
	{
		cmatch acdata;
		if (regex_match(FlightPlan.GetFlightPlanData().GetAircraftInfo(), acdata, regex("(\\w{2,4})\\/([LMHJ])-(\\w+)\\/(\\w*?[" + CodesICAO + "]\\w*)", std::regex::icase)))
			return true;
	}

	//check for FAA suffix
	if (acceptEquipmentFAA)
	{
		if (EquipmentCodesFAA.find(FlightPlan.GetFlightPlanData().GetCapibilities()) != string::npos)
			return true;
	}

	return false;
}

bool CCAMS::IsEligibleSquawkModeS(const EuroScopePlugIn::CFlightPlan& FlightPlan) const
{
	return IsAcModeS(FlightPlan) && IsApModeS(FlightPlan.GetFlightPlanData().GetDestination()) && IsRteModeS(FlightPlan) &&
		(IsApModeS(FlightPlan.GetFlightPlanData().GetOrigin()) || 
			(!IsADEPvicinity(FlightPlan) && (strlen(FlightPlan.GetTrackingControllerCallsign()) > 0) ? IsApModeS(FlightPlan.GetTrackingControllerCallsign()) : IsApModeS(ControllerMyself().GetCallsign())));
}

bool CCAMS::HasDuplicateSquawk(const CRadarTarget& RadarTarget)
{
	const char* pssr = RadarTarget.GetPosition().GetSquawk();
	string DisplayMsg;

	for (CRadarTarget RT = RadarTargetSelectFirst(); RT.IsValid();
		RT = RadarTargetSelectNext(RT))
	{
		if (strcmp(RT.GetCallsign(), RadarTarget.GetCallsign()) == 0)
			continue;

		if (strcmp(pssr, RT.GetPosition().GetSquawk()) == 0)
		{
			// duplicate identified for the actual set code
#ifdef _DEBUG
			DisplayMsg = "DUPE: SET code " + string{ pssr } + " of " + RadarTarget.GetCallsign() + " is already used (SET) by " + RT.GetCallsign();
			DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
			return true;
		}
	}

	// searching for duplicate assignments in flight plans
	for (CFlightPlan FP = FlightPlanSelectFirst(); FP.IsValid(); FP = FlightPlanSelectNext(FP))
	{
		if (strcmp(FP.GetCallsign(), RadarTarget.GetCallsign()) == 0 || strlen(FP.GetControllerAssignedData().GetSquawk()) != 4)
			continue;

		if (strcmp(pssr, FP.GetControllerAssignedData().GetSquawk()) == 0)
		{
			// duplicate identified for the actual set code
#ifdef _DEBUG
			DisplayMsg = "DUPE: SET code " + string{ pssr } + " of " + RadarTarget.GetCallsign() + " is already assigned to " + FP.GetCallsign();
			DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
			return true;
		}
	}
	return false;
}

bool CCAMS::HasDuplicateSquawk(const CFlightPlan& FlightPlan)
{
	const char* assr = FlightPlan.GetControllerAssignedData().GetSquawk();
	string DisplayMsg;

	if (strlen(assr) == 4)
	{
		if (atoi(assr) % 100 == 0)
			return false;
		else if (strcmp(FlightPlan.GetFlightPlanData().GetPlanType(), "V") == 0)
			return false;

		// searching for duplicate assignments in radar targets
		for (CRadarTarget RT = RadarTargetSelectFirst(); RT.IsValid();
			RT = RadarTargetSelectNext(RT))
		{
			if (strcmp(RT.GetCallsign(), FlightPlan.GetCallsign()) == 0)
				continue;

			if (strcmp(assr, RT.GetPosition().GetSquawk()) == 0)
			{
				// duplicate identified for the assigned code
#ifdef _DEBUG
				DisplayMsg = "DUPE: ASSIGNED code " + string{ assr } + " of " + FlightPlan.GetCallsign() + " is already used (SET) by " + RT.GetCallsign();
				DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
				return true;
			}
		}

		// searching for duplicate assignments in flight plans
		for (CFlightPlan FP = FlightPlanSelectFirst(); FP.IsValid(); FP = FlightPlanSelectNext(FP))
		{
			if (strcmp(FP.GetCallsign(), FlightPlan.GetCallsign()) == 0 || strlen(FP.GetControllerAssignedData().GetSquawk()) != 4)
				continue;

			if (strcmp(assr, FP.GetControllerAssignedData().GetSquawk()) == 0)
			{
				// duplicate identified for the assigned code
#ifdef _DEBUG
				DisplayMsg = "DUPE: ASSIGNED code " + string{ assr } + " of " + FlightPlan.GetCallsign() + " is already assigned to " + FP.GetCallsign();
				DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
				return true;
			}
		}
	}

	return false;
}

bool CCAMS::HasDuplicatePSSR(const CFlightPlan& FlightPlan)
{
	const char* assr = FlightPlan.GetControllerAssignedData().GetSquawk();
	string DisplayMsg;

	if (strlen(assr) == 4)
	{
		// searching for duplicate assignments in radar targets
		for (CRadarTarget RT = RadarTargetSelectFirst(); RT.IsValid();
			RT = RadarTargetSelectNext(RT))
		{
			if (strcmp(RT.GetCallsign(), FlightPlan.GetCallsign()) == 0)
				continue;

			if (strcmp(assr, RT.GetPosition().GetSquawk()) == 0)
			{
				// duplicate identified for the assigned code
#ifdef _DEBUG
				DisplayMsg = "DUPE: ASSIGNED code " + string{ assr } + " of " + FlightPlan.GetCallsign() + " is already used (SET) by " + RT.GetCallsign();
				DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
				return true;
			}
		}
	}

	return false;
}

bool CCAMS::HasValidSquawk(const EuroScopePlugIn::CFlightPlan& FlightPlan)
{
	const char* assr = FlightPlan.GetControllerAssignedData().GetSquawk();
	const char* pssr = FlightPlan.GetCorrelatedRadarTarget().GetPosition().GetSquawk();
	string DisplayMsg;

#if _DEBUG
	DisplayMsg = string("Controller " + (string)ControllerMyself().GetCallsign() + ", Is mode S: " + (IsApModeS(ControllerMyself().GetCallsign()) ? "True" : "False"));
	//DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif // _DEBUG


	if ((strcmp(FlightPlan.GetFlightPlanData().GetPlanType(), "V") == 0 && (strcmp(assr, squawkVFR) == 0 || strcmp(pssr, squawkVFR) == 0))
		|| (IsEligibleSquawkModeS(FlightPlan) && (strcmp(assr, squawkModeS) == 0 || strcmp(pssr, squawkModeS) == 0)))
	{
		return true;
	}
	else if (strlen(assr) == 4)
	{
		// assigned squawk is not valid
		if (!regex_match(assr, std::regex("[0-7]{4}")) || atoi(assr) % 100 == 0)
		{
#ifdef _DEBUG
			DisplayMsg = "ASSIGNED code " + string{ assr } + " is not valid for " + FlightPlan.GetCallsign();
			//DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
			return false;
		}
	}
	else if (!regex_match(pssr, std::regex("[0-7]{4}")) || atoi(pssr) % 100 == 0)
	{
		// no squawk is assigned, but currently used code is not valid
		{
#ifdef _DEBUG
			DisplayMsg = "SET code " + string{ pssr } + " is not valid for " + FlightPlan.GetCallsign();
			//DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
			return false;
		}
	}
	
	if (HasDuplicateSquawk(FlightPlan))
		return false;

	// no duplicate with assigend or used codes has been found
#ifdef _DEBUG
	DisplayMsg = "No duplicates found for " + string{ FlightPlan.GetCallsign() } + " (ASSIGNED '" + assr + "', SET code " + pssr + ")";
	//DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
	return true;
}

std::vector<string> CCAMS::collectUsedCodes(const CFlightPlan& FlightPlan)
{
	vector<string> usedCodes;
	int i = 0;
	for (CRadarTarget RadarTarget = RadarTargetSelectFirst(); RadarTarget.IsValid();
		RadarTarget = RadarTargetSelectNext(RadarTarget))
	{
		i++;
		if (RadarTarget.GetCallsign() == FlightPlan.GetCallsign())
		{
#ifdef _DEBUG
			string DisplayMsg{ "The code of " + (string)RadarTarget.GetCallsign() + " is not considered" };
			//DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
			continue;
		}

		// search for all actual codes used by pilots
		auto pssr = RadarTarget.GetPosition().GetSquawk();
		if (strlen(pssr) == 4 &&
			atoi(pssr) % 100 != 0 &&
			strcmp(pssr, squawkModeS) != 0 &&
			strcmp(pssr, squawkVFR) != 0 &&
			strcmp(pssr, RadarTarget.GetCorrelatedFlightPlan().GetControllerAssignedData().GetSquawk()) != 0)
		{
			usedCodes.push_back(pssr);
		}
	}

	int j = 0;
	for (CFlightPlan FP = FlightPlanSelectFirst(); FP.IsValid(); FP = FlightPlanSelectNext(FP))
	{
		j++;
		if (FP.GetCallsign() == FlightPlan.GetCallsign())
		{
#ifdef _DEBUG
			string DisplayMsg{ "The code of " + (string)FP.GetCallsign() + " is not considered" };
			//DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
			continue;
		}

		// search for all controller assigned codes
		auto assr = FP.GetControllerAssignedData().GetSquawk();
		if (strlen(assr) == 4 &&
			atoi(assr) % 100 != 0 &&
			strcmp(assr, squawkModeS) != 0 &&
			strcmp(assr, squawkVFR) != 0)
		{
			usedCodes.push_back(assr);
		}

	}
#ifdef _DEBUG
	DisplayUserMessage(MY_PLUGIN_NAME, "Debug", string{"Used Codes: " + to_string(i) + " RadarTargets, " + to_string(j) + " FlightPlans"}.c_str(), true, false, false, false, false);
#endif // _DEBUG


	sort(usedCodes.begin(), usedCodes.end());
	auto u = unique(usedCodes.begin(), usedCodes.end());
	usedCodes.erase(u, usedCodes.end());

	return usedCodes;
}

#ifdef _DEBUG

void CCAMS::writeLogFile(stringstream& sText)
{
	ofstream file;
	time_t rawtime;
	struct tm timeinfo;
	char timestamp[256];

	time(&rawtime);
	localtime_s(&timeinfo, &rawtime);
	strftime(timestamp, sizeof(timestamp), "%Y-%m-%d", &timeinfo);

	file.open((MY_PLUGIN_NAME + string("_") + string(timestamp) + ".log").c_str(), ofstream::out | ofstream::app);

	strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
	file << timestamp << ":" << sText.str() << endl;
	file.close();
}

#endif // _DEBUG
