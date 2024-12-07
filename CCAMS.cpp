#include "stdafx.h"
#include "CCAMS.h"
#include <fstream>

CCAMS::CCAMS(const EquipmentCodes&& ec, const SquawkCodes&& sc) : CPlugIn(EuroScopePlugIn::COMPATIBILITY_CODE,
	MY_PLUGIN_NAME,
	MY_PLUGIN_VERSION,
	MY_PLUGIN_DEVELOPER,
	MY_PLUGIN_COPYRIGHT),
	EquipmentCodesFAA(ec.FAA),
	EquipmentCodesICAO(ec.ICAO_MODE_S),
	EquipmentCodesICAOEHS(ec.ICAO_EHS),
	ModeSAirports(MODE_S_AIRPORTS),
	squawkModeS(sc.MODE_S),
	squawkVFR(sc.VFR)
{
	string DisplayMsg { "Version " + string { MY_PLUGIN_VERSION } + " loaded" };
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

	RegisterTagItemFunction("Auto assign squawk", ItemCodes::TAG_FUNC_ASSIGN_SQUAWK_AUTO);
	RegisterTagItemFunction("Open SQUAWK assign popup", ItemCodes::TAG_FUNC_SQUAWK_POPUP);

	FpListEHS = RegisterFpList("Mode S EHS");
	if (FpListEHS.GetColumnNumber() == 0)
	{
		FpListEHS.AddColumnDefinition("C/S", 8, false, NULL, TAG_ITEM_TYPE_CALLSIGN, NULL, TAG_ITEM_FUNCTION_OPEN_FP_DIALOG, NULL, NULL);
		FpListEHS.AddColumnDefinition("HDG", 5, true, MY_PLUGIN_NAME, ItemCodes::TAG_ITEM_EHS_HDG, NULL, NULL, NULL, NULL);
		FpListEHS.AddColumnDefinition("Roll", 5, true, MY_PLUGIN_NAME, ItemCodes::TAG_ITEM_EHS_ROLL, NULL, NULL, NULL, NULL);
		FpListEHS.AddColumnDefinition("GS", 4, true, MY_PLUGIN_NAME, ItemCodes::TAG_ITEM_EHS_GS, NULL, NULL, NULL, NULL);
	}

	// Start new thread to get the version file from the server
	fUpdateString = async(LoadUpdateString);

	// Set default setting values
	ConnectionStatus = 0;
	pluginVersionCheck = false;
	acceptEquipmentICAO = true;
	acceptEquipmentFAA = true;
#ifdef _DEBUG
	autoAssign = true;
#else
	autoAssign = true;
#endif
	APTcodeMaxGS = 50;
	APTcodeMaxDist = 3;
	
	ReadSettings();
}

CCAMS::~CCAMS()
{}

bool CCAMS::OnCompileCommand(const char* command)
{
	string commandString(command);
	cmatch matches;

	if (_stricmp(command, ".help") == 0)
	{
		DisplayUserMessage("HELP", "HELP", ".HELP CCAMS | Centralised code assignment and management system Help", true, true, true, true, false);
		return NULL;
	}
	else if (_stricmp(command, ".help ccams") == 0)
	{
		// Display HELP
		DisplayUserMessage("HELP", MY_PLUGIN_NAME, ".CCAMS EHSLIST | Displays the flight plan list with EHS values of the currently selected aircraft.", true, true, true, true, false);
		DisplayUserMessage("HELP", MY_PLUGIN_NAME, ".CCAMS AUTO | Activates or deactivates automatic code assignment.", true, true, true, true, false);
		DisplayUserMessage("HELP", MY_PLUGIN_NAME, ".CCAMS TRACKING | Activates or deactivates transponder code validation when starting to track a flight.", true, true, true, true, false);
		DisplayUserMessage("HELP", MY_PLUGIN_NAME, ".CCAMS RELOAD | Force load of local and remote settings.", true, true, true, true, false);
#ifdef _DEBUG
		DisplayUserMessage("HELP", MY_PLUGIN_NAME, ".CCAMS RESET | Clears the list of flight plans which have been determined no longer applicable for automatic code assignment.", true, true, true, true, false);
		DisplayUserMessage("HELP", MY_PLUGIN_NAME, ".CCAMS [CALL SIGN] | Displays tracking and controller information for a specific flight (to support debugging of automatic code assignment).", true, true, true, true, false);
#endif
		return true;
	}
	else if (regex_search(command, matches, regex("^\\.ccams\\s+(\\w+)", regex::icase)))
	{
		return PluginCommands(matches[1].str().c_str());
	}

	return false;
}

bool CCAMS::PluginCommands(const char* Command)
{
	//string sCommand(Command);
	if (_stricmp(Command, "ehslist") == 0)
	{
		FpListEHS.ShowFpList(true);
		return true;
	}
	else if (_stricmp(Command, "auto") == 0)
	{
		if (!pluginVersionCheck)
		{
			DisplayUserMessage(MY_PLUGIN_NAME, "Error", "Your plugin version is not up-to-date and the automatic code assignment therefore not available.", true, true, false, false, false);
		}
		else if (autoAssign)
		{
			autoAssign = false;
			SaveDataToSettings("AutoAssign", "Automatic assignment of squawk codes", "0");
			DisplayUserMessage(MY_PLUGIN_NAME, "Setting changed", "Automatic code assignment disabled", true, true, false, false, false);
		}
		else
		{
			autoAssign = true;
			SaveDataToSettings("AutoAssign", "Automatic assignment of squawk codes", "1");
			DisplayUserMessage(MY_PLUGIN_NAME, "Setting changed", "Automatic code assignment enabled", true, true, false, false, false);
		}
		return true;
	}
	else if (_stricmp(Command, "tracking") == 0)
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
	else if (_stricmp(Command, "reload") == 0)
	{
		fUpdateString = async(LoadUpdateString);
		ReadSettings();
		return true;
	}
#ifdef _DEBUG
	else if (_stricmp(Command, "reset") == 0)
	{
		ProcessedFlightPlans.clear();
		return true;
	}
	else if (_stricmp(Command, "list") == 0)
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
	else if (_stricmp(Command, "sqlist") == 0)
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
			DisplayUserMessage("CCAMS Squawk List Dump", RadarTarget.GetCallsign(), DisplayMsg.c_str(), true, false, false, false, false);
		}
		return true;
	}
	else
	{
		for (CFlightPlan FlightPlan = FlightPlanSelectFirst(); FlightPlan.IsValid();
			FlightPlan = FlightPlanSelectNext(FlightPlan))
		{
			if (_stricmp(Command, FlightPlan.GetCallsign()) == 0)
			{
				string DisplayMsg = "Status " + string{FlightPlan.GetCallsign()} + ": " + (FlightPlan.GetSimulated() ? "simulated" : "not sim") + 
				", FP Type '" + FlightPlan.GetFlightPlanData().GetPlanType() + "'" + 
				", AC info '" + FlightPlan.GetFlightPlanData().GetAircraftInfo() + "' / '" + FlightPlan.GetFlightPlanData().GetCapibilities() + "'" +
				", " + to_string(FlightPlan.GetSectorEntryMinutes()) + " Minutes to Sector Entry, " + 
				(HasValidSquawk(FlightPlan) ? "has valid squawk" : "has NO valid squawk") +
				", ASSIGNED '" + FlightPlan.GetControllerAssignedData().GetSquawk() + "', SET " + FlightPlan.GetCorrelatedRadarTarget().GetPosition().GetSquawk();
				DisplayUserMessage("CCAMS FP Status", "Debug", DisplayMsg.c_str(), true, false, false, false, false);

				if (!ControllerMyself().IsValid() || !ControllerMyself().IsController() || (ControllerMyself().GetFacility() > 1 && ControllerMyself().GetFacility() < 5))
					DisplayUserMessage("CCAMS FP Status", "Debug", "This controller is not allowed to automatically assign squawks", true, false, false, false, false);


				if (find(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()) != ProcessedFlightPlans.end())
					DisplayUserMessage("CCAMS FP Status", "Debug", "This flight plan has already been processed", true, false, false, false, false);

				for (CRadarTarget RadarTarget = RadarTargetSelectFirst(); RadarTarget.IsValid();
					RadarTarget = RadarTargetSelectNext(RadarTarget))
				{
					if (_stricmp(RadarTarget.GetCallsign(), FlightPlan.GetCallsign()) == 0)
						continue;
					else if (_stricmp(RadarTarget.GetCorrelatedFlightPlan().GetControllerAssignedData().GetSquawk(), FlightPlan.GetControllerAssignedData().GetSquawk()) == 0 && _stricmp(RadarTarget.GetCorrelatedFlightPlan().GetControllerAssignedData().GetSquawk(),squawkModeS) != 0)
					{
						DisplayMsg = "ASSR also used for " + string{ RadarTarget.GetCallsign() } + ", " + (RadarTarget.GetCorrelatedFlightPlan().GetSimulated() ? "simulated" : "not sim") +
							", FP Type '" + RadarTarget.GetCorrelatedFlightPlan().GetFlightPlanData().GetPlanType() + "', " + to_string(RadarTarget.GetCorrelatedFlightPlan().GetSectorEntryMinutes()) +
							" Minutes to Sector Entry, " + (HasValidSquawk(RadarTarget.GetCorrelatedFlightPlan()) ? "has valid squawk" : "has NO valid squawk") + 
							", ASSIGNED '" + RadarTarget.GetCorrelatedFlightPlan().GetControllerAssignedData().GetSquawk() + "', SET " + RadarTarget.GetPosition().GetSquawk();
						DisplayUserMessage("CCAMS FP Status", RadarTarget.GetCallsign(), DisplayMsg.c_str(), true, false, false, false, false);
					}
					else if (_stricmp(RadarTarget.GetPosition().GetSquawk(), FlightPlan.GetControllerAssignedData().GetSquawk()) == 0 && _stricmp(RadarTarget.GetPosition().GetSquawk(), squawkModeS) != 0)
					{
						DisplayMsg = "PSSR also used for " + string{ RadarTarget.GetCallsign() } + ", " + (RadarTarget.GetCorrelatedFlightPlan().GetSimulated() ? "simulated" : "not sim") +
							", FP Type '" + RadarTarget.GetCorrelatedFlightPlan().GetFlightPlanData().GetPlanType() + "', " + to_string(RadarTarget.GetCorrelatedFlightPlan().GetSectorEntryMinutes()) +
							" Minutes to Sector Entry, " + (HasValidSquawk(RadarTarget.GetCorrelatedFlightPlan()) ? "has valid squawk" : "has NO valid squawk") + 
							", ASSIGNED '" + RadarTarget.GetCorrelatedFlightPlan().GetControllerAssignedData().GetSquawk() + "', SET " + RadarTarget.GetPosition().GetSquawk();
						DisplayUserMessage("CCAMS FP Status", RadarTarget.GetCallsign(), DisplayMsg.c_str(), true, false, false, false, false);
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
			if (IsEHS(FlightPlan))
			{
				//strncpy(sItemString, to_string(RadarTarget.GetPosition().GetReportedHeading()).c_str(), 16);
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

			if (!IsEligibleSquawkModeS(FlightPlan) && (strcmp(assr, squawkModeS) == 0 || (strcmp(pssr, squawkModeS) == 0 && strlen(assr) == 0)))
			{
				// mode S code assigned, but not eligible
				*pColorCode = EuroScopePlugIn::TAG_COLOR_REDUNDANT;
			}
			else if(strcmp(assr, pssr) != 0)
			{
				// assigned squawk is not set
				*pColorCode = EuroScopePlugIn::TAG_COLOR_INFORMATION;
			}
			strcpy_s(sItemString, 16, assr);
		}

	}
}

void CCAMS::OnFlightPlanDisconnect(CFlightPlan FlightPlan)
{
	ProcessedFlightPlans.erase(remove(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()),
		ProcessedFlightPlans.end());

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
	else if (FlightPlan.GetTrackingControllerIsMe())
	{
		if (autoAssign && pluginVersionCheck && ConnectionStatus > 10)
		{
#ifdef _DEBUG
			log << FlightPlan.GetCallsign() << ":FP processed for automatic squawk assignment:flight plan update and controller is tracking";
			writeLogFile(log);
			string DisplayMsg = string{ FlightPlan.GetCallsign() } + " is processed for automatic squawk assignment (due to flight plan update and controller is tracking)";
			DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, true, false, false, false);
#endif
			AssignAutoSquawk(FlightPlan);
		}
	}
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
			//ProcessedFlightPlans.erase(it);
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
			FpListEHS.RemoveFpFromTheList(FP);
		}
		FpListEHS.AddFpToTheList(FlightPlanSelectASEL());
	}
}

void CCAMS::OnFunctionCall(int FunctionId, const char* sItemString, POINT Pt, RECT Area)
{
	CFlightPlan FlightPlan = FlightPlanSelectASEL();
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
			if (PendingSquawks.find(FlightPlan.GetCallsign()) == PendingSquawks.end())
			{
				PendingSquawks.insert(std::make_pair(FlightPlan.GetCallsign(), std::async(LoadWebSquawk,
					FlightPlan, ControllerMyself(), collectUsedCodes(FlightPlan), IsADEPvicinity(FlightPlan), GetConnectionType())));
#ifdef _DEBUG
				if (GetConnectionType() > 2)
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
	if (fUpdateString.valid() && fUpdateString.wait_for(0ms) == future_status::ready)
		DoInitialLoad(fUpdateString);

	if (GetConnectionType() > 0)
		ConnectionStatus++;
	else if (GetConnectionType() != ConnectionStatus)
	{
		ConnectionStatus = 0;
		if (ProcessedFlightPlans.size() > 0)
		{
			ProcessedFlightPlans.clear();
#ifdef _DEBUG
			DisplayUserMessage(MY_PLUGIN_NAME, "Debug", "Connection Status 0 detected, all processed flight plans are removed from the list", true, false, false, false, false);
#endif
		}
	}

#ifdef _DEBUG
	if (ConnectionStatus == 10)
		DisplayUserMessage(MY_PLUGIN_NAME, "Debug", "Active connection established, automatic squawk assignment enabled", true, false, false, false, false);
#endif


	if (ControllerMyself().IsValid() && ControllerMyself().IsController())
	{
		AssignPendingSquawks();

		if (!(Counter % 3) && autoAssign && pluginVersionCheck && ConnectionStatus > 10)
		{
#ifdef _DEBUG
			DisplayUserMessage(MY_PLUGIN_NAME, "Debug", "Starting timer-based auto-assignments", true, false, false, false, false);
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
	if (!ControllerMyself().IsValid() || !ControllerMyself().IsController() || ControllerMyself().GetRating() < 2 || (ControllerMyself().GetFacility() > 1 && ControllerMyself().GetFacility() < 5))
		return;

//	// check for exclusion arguments from automatic squawk assignment
//	if (find(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()) != ProcessedFlightPlans.end())
//	{
//		// this flight was already processed
//		if (FlightPlan.GetSimulated() || strcmp(FlightPlan.GetFlightPlanData().GetPlanType(), "V") == 0)
//			return;
//		else if (FlightPlan.GetFlightPlanData().IsReceived() && FlightPlan.GetSectorEntryMinutes() < 0)
//			return;
//		else if (HasValidSquawk(FlightPlan))
//			return;
//
//		// The flight was already processed, but the assigned code has become invalid again
//		// This is probably due to a duplicate, where the code assigned earlier was assigned to a second aircraft by another controller
//		else if (FlightPlan.GetTrackingControllerIsMe())
//		{
//			// attempting to change to squawk of the other aircraft
//			for (CRadarTarget RadarTarget = RadarTargetSelectFirst(); RadarTarget.IsValid();
//				RadarTarget = RadarTargetSelectNext(RadarTarget))
//			{
//				if (_stricmp(RadarTarget.GetCallsign(), FlightPlan.GetCallsign()) == 0)
//					continue;
//				else if (_stricmp(RadarTarget.GetCorrelatedFlightPlan().GetControllerAssignedData().GetSquawk(), FlightPlan.GetControllerAssignedData().GetSquawk()) == 0
//					&& RadarTarget.GetCorrelatedFlightPlan().GetTrackingControllerCallsign() > 0)
//				{
//					PendingSquawks.insert(std::make_pair(RadarTarget.GetCallsign(), std::async(LoadWebSquawk,
//						RadarTarget.GetCorrelatedFlightPlan(), ControllerMyself(), collectUsedCodes(RadarTarget.GetCorrelatedFlightPlan()), IsADEPvicinity(RadarTarget.GetCorrelatedFlightPlan()), GetConnectionType())));
//#ifdef _DEBUG
//					log << RadarTarget.GetCallsign() << ":duplicate assigned code:unique code AUTO assigned:" << FlightPlan.GetCallsign() << " already tracked by " << FlightPlan.GetTrackingControllerCallsign();
//					writeLogFile(log);
//					DisplayMsg = string{ RadarTarget.GetCallsign() } + ", unique code AUTO assigned due to a detected duplicate with " + FlightPlan.GetCallsign();
//					DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
//#endif
//				}
//			}
//			return;
//		}
//
//		// removing the call sign from the processed flight plan list to initiate a new assignment
//		ProcessedFlightPlans.erase(remove(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()), ProcessedFlightPlans.end());
//#ifdef _DEBUG
//		log << FlightPlan.GetCallsign() << ":duplicate assigned code:FP removed from processed list";
//		writeLogFile(log);
//		string DisplayMsg = string{ FlightPlan.GetCallsign() } + " removed from processed list because the assigned code is no longer valid";
//		DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
//#endif
//	}

	if (FlightPlan.GetSimulated() || strcmp(FlightPlan.GetFlightPlanData().GetPlanType(), "V") == 0)
	{
		// disregard simulated flight plans (out of the controllers range)
		// disregard flight with flight rule VFR
		if (find(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()) != ProcessedFlightPlans.end())
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
		if (find(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()) != ProcessedFlightPlans.end())
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
		// this flight has already assigned a valid unique code
		if (FlightPlan.GetTrackingControllerIsMe())
		{
			{
				if (find(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()) != ProcessedFlightPlans.end())
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
	else if (find(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()) != ProcessedFlightPlans.end())
	{
		// The flight was already processed, but the assigned code has become invalid again
		// This is probably due to a duplicate, where the code assigned earlier was assigned to a second aircraft by another controller
		if (FlightPlan.GetTrackingControllerIsMe())
		{
			// attempting to change to squawk of the other aircraft
			for (CRadarTarget RadarTarget = RadarTargetSelectFirst(); RadarTarget.IsValid();
				RadarTarget = RadarTargetSelectNext(RadarTarget))
			{
				if (_stricmp(RadarTarget.GetCallsign(), FlightPlan.GetCallsign()) == 0)
					continue;
				else if (_stricmp(RadarTarget.GetCorrelatedFlightPlan().GetControllerAssignedData().GetSquawk(), FlightPlan.GetControllerAssignedData().GetSquawk()) == 0
					&& strlen(RadarTarget.GetCorrelatedFlightPlan().GetTrackingControllerCallsign()) > 0)
				{
					PendingSquawks.insert(std::make_pair(RadarTarget.GetCallsign(), std::async(LoadWebSquawk,
						RadarTarget.GetCorrelatedFlightPlan(), ControllerMyself(), collectUsedCodes(RadarTarget.GetCorrelatedFlightPlan()), IsADEPvicinity(RadarTarget.GetCorrelatedFlightPlan()), GetConnectionType())));
#ifdef _DEBUG
					log << RadarTarget.GetCallsign() << ":duplicate assigned code:unique code AUTO assigned:" << FlightPlan.GetCallsign() << " already tracked by " << FlightPlan.GetTrackingControllerCallsign();
					writeLogFile(log);
					DisplayMsg = string{ RadarTarget.GetCallsign() } + ", unique code AUTO assigned due to a detected duplicate with " + FlightPlan.GetCallsign();
					DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
				}
			}
			return;
		}

		// removing the call sign from the processed flight plan list to initiate a new assignment
		ProcessedFlightPlans.erase(remove(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()), ProcessedFlightPlans.end());
#ifdef _DEBUG
		log << FlightPlan.GetCallsign() << ":duplicate assigned code:FP removed from processed list";
		writeLogFile(log);
		string DisplayMsg = string{ FlightPlan.GetCallsign() } + " removed from processed list because the assigned code is no longer valid";
		DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
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
	else
	{
		PendingSquawks.insert(std::make_pair(FlightPlan.GetCallsign(), std::async(LoadWebSquawk,
			FlightPlan, ControllerMyself(), collectUsedCodes(FlightPlan), IsADEPvicinity(FlightPlan), GetConnectionType())));
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
	future<string> webSquawk = std::async(LoadWebSquawk, FlightPlan, ControllerMyself(), collectUsedCodes(FlightPlan), IsADEPvicinity(FlightPlan), GetConnectionType());

	if (webSquawk.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
	{
		string squawk = webSquawk.get();
		if (!FlightPlanSelect(FlightPlan.GetCallsign()).GetControllerAssignedData().SetSquawk(squawk.c_str()))
		{
			PendingSquawks.insert(std::make_pair(FlightPlan.GetCallsign(), std::async(LoadWebSquawk,
				FlightPlan, ControllerMyself(), collectUsedCodes(FlightPlan), IsADEPvicinity(FlightPlan), GetConnectionType())));
		}
	}
}

void CCAMS::AssignPendingSquawks()
{
	for (auto it = PendingSquawks.begin(), next_it = it; it != PendingSquawks.end(); it = next_it)
	{
		bool must_delete = false;
		if (it->second.valid() && it->second.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
			std::string squawk = it->second.get();
			//if (squawk is an error number)
#ifdef _DEBUG
			string DisplayMsg = string{ it->first } + ", code " + squawk + " assigned";
			DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
			if (!FlightPlanSelect(it->first).GetControllerAssignedData().SetSquawk(squawk.c_str()))
			{
				if (squawk == "E404")
				{
					DisplayUserMessage(MY_PLUGIN_NAME, "Error 404", "The internet connection cannot be initiated", true, true, false, false, false);
				}
				else if (squawk == "E406")
				{
					DisplayUserMessage(MY_PLUGIN_NAME, "Error 406", "No answer received from the CCAMS server", true, true, false, false, false);
				}
				else
				{
					string DisplayMsg{ "Your request for a squawk from the centralised code server failed. Check your plugin version, try again or revert to the ES built-in functionalities for assigning a squawk (F9)." };
					DisplayUserMessage(MY_PLUGIN_NAME, "Error", DisplayMsg.c_str(), true, true, false, false, false);
					DisplayUserMessage(MY_PLUGIN_NAME, "Error", ("For troubleshooting, report code '" + squawk + "'").c_str(), true, true, false, false, false);
				}
			}
			must_delete = true;
		}

		++next_it;
		if (must_delete)
		{
			PendingSquawks.erase(it);
		}
	}
}

void CCAMS::DoInitialLoad(future<string> & fmessage)
{
	try
	{
		string message = fmessage.get();
		smatch match;
#ifdef _DEBUG
		string DisplayMsg = "Update string downloaded: " + message;
		DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
		if (regex_match(message, match, regex("(\\d+)[:](\\d+)[:]([A-Z,]+)[:]([^:]+)", regex::icase)))
		{
			if (stoi(match[2].str(), nullptr, 0) > MY_PLUGIN_VERSIONCODE)
				throw error{ "Your " + string { MY_PLUGIN_NAME } + " plugin (version " + MY_PLUGIN_VERSION + ") is outdated and the automatic code assignment therefore not available. Please change to the latest version.\n\nVisit https://github.com/kusterjs/CCAMS/releases" };
			else
				pluginVersionCheck = true;

			if (stoi(match[1].str(), nullptr, 0) > MY_PLUGIN_VERSIONCODE)
			{
				DisplayUserMessage(MY_PLUGIN_NAME, "Update", "An update for the CCAMS plugin is available. Please visit https://github.com/kusterjs/CCAMS/releases and download the latest version.", true, true, false, true, false);
			}
			
			EquipmentCodesFAA = match[3].str();
			ModeSAirports = regex(match[4].str(), regex::icase);
		}
		else
		{
			throw error{ string { MY_PLUGIN_NAME }  + " plugin couldn't parse the server configuration and version data. Automatic code assignment therefore not available." };
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
				autoAssign = false;
			}
			else if (strcmp(cstrSetting, "1") == 0)
			{
				autoAssign = true;
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
	//if (find(ProcessedFlightPlans.begin(), ProcessedFlightPlans.end(), FlightPlan.GetCallsign()) != ProcessedFlightPlans.end())
	//	return true;

	//return false;

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
	if (regex_search(icao, ModeSAirports))
		return true;

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
	//return isAcModeS(FlightPlan) && isApModeS(FlightPlan.GetFlightPlanData().GetDestination()) &&
	//	(isApModeS(FlightPlan.GetFlightPlanData().GetOrigin()) || !isADEPvicinity(FlightPlan));
	return IsAcModeS(FlightPlan) && IsApModeS(FlightPlan.GetFlightPlanData().GetDestination()) &&
		(IsApModeS(FlightPlan.GetFlightPlanData().GetOrigin()) || (!IsADEPvicinity(FlightPlan) && (strlen(FlightPlan.GetTrackingControllerCallsign()) > 0) ? IsApModeS(FlightPlan.GetTrackingControllerCallsign()) : IsApModeS(ControllerMyself().GetCallsign())));
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

	// searching for duplicate assignments in radar targets
	for (CRadarTarget RadarTarget = RadarTargetSelectFirst(); RadarTarget.IsValid();
		RadarTarget = RadarTargetSelectNext(RadarTarget))
	{
		if (strcmp(RadarTarget.GetCallsign(),FlightPlan.GetCallsign()) == 0)
			continue;

		if (strlen(assr) == 4)
		{
			if (strcmp(assr, RadarTarget.GetCorrelatedFlightPlan().GetControllerAssignedData().GetSquawk()) == 0 || strcmp(assr, RadarTarget.GetPosition().GetSquawk()) == 0)
			{
				// duplicate identified for the assigned code
#ifdef _DEBUG
				DisplayMsg = "ASSIGNED code " + string{ assr } + " of " + FlightPlan.GetCallsign() + " is already used by " + RadarTarget.GetCallsign();
				DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
				return false;
			}
		}
		else
		{
			if (strcmp(pssr, RadarTarget.GetCorrelatedFlightPlan().GetControllerAssignedData().GetSquawk()) == 0 || strcmp(pssr, RadarTarget.GetPosition().GetSquawk()) == 0)
			{
				// duplicate identified for the actual set code
#ifdef _DEBUG
				DisplayMsg = "SET code '" + string{ pssr } + "' of " + FlightPlan.GetCallsign() + " is already used by " + RadarTarget.GetCallsign();
				DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
				return false;
			}
			else
			{
				// as an option, if no code has been assigned and the currently used one has not been identified as a dpublicate, it could be set as the assigned code
				//FlightPlan.GetControllerAssignedData().SetSquawk(pssr);
			}
		}
	}

	// searching for duplicate assignments in flight plans
	for (CFlightPlan FP = FlightPlanSelectFirst(); FP.IsValid(); FP = FlightPlanSelectNext(FP))
	{
		if (strcmp(FP.GetCallsign(), FlightPlan.GetCallsign()) == 0)
			continue;

		if (strlen(assr) == 4)
		{
			if (strcmp(assr, FP.GetControllerAssignedData().GetSquawk()) == 0 || strcmp(assr, FP.GetCorrelatedRadarTarget().GetPosition().GetSquawk()) == 0)
			{
				// duplicate identified for the assigned code
#ifdef _DEBUG
				DisplayMsg = "ASSIGNED code " + string{ assr } + " of " + FlightPlan.GetCallsign() + " is already used by " + FP.GetCallsign();
				DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
				return false;
			}
		}
		else
		{
			if (strcmp(pssr, FP.GetControllerAssignedData().GetSquawk()) == 0 || strcmp(pssr, FP.GetCorrelatedRadarTarget().GetPosition().GetSquawk()) == 0)
			{
				// duplicate identified for the actual set code
#ifdef _DEBUG
				DisplayMsg = "SET code '" + string{ pssr } + "' of " + FlightPlan.GetCallsign() + " is already used by " + FP.GetCallsign();
				DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
				return false;
			}
			else
			{
				// as an option, if no code has been assigned and the currently used one has not been identified as a dpublicate, it could be set as the assigned code
				//FlightPlan.GetControllerAssignedData().SetSquawk(pssr);
			}
		}
	}

	// no duplicate with assigend or used codes has been found
#ifdef _DEBUG
	DisplayMsg = "No duplicates found for " + string{ FlightPlan.GetCallsign() } + " (ASSIGNED '" + assr + "', SET code " + pssr + ")";
	//DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
	return true;
}

std::vector<const char*> CCAMS::collectUsedCodes(const CFlightPlan& FlightPlan)
{
	vector<const char*> usedCodes;
	for (CRadarTarget RadarTarget = RadarTargetSelectFirst(); RadarTarget.IsValid();
		RadarTarget = RadarTargetSelectNext(RadarTarget))
	{
		if (RadarTarget.GetCallsign() == FlightPlanSelectASEL().GetCallsign())
		{
#ifdef _DEBUG
			string DisplayMsg{ "The code of " + (string)RadarTarget.GetCallsign() + " is not considered" };
			//DisplayUserMessage(MY_PLUGIN_NAME, "Debug", DisplayMsg.c_str(), true, false, false, false, false);
#endif
			continue;
		}

		// search for all controller assigned codes
		auto assr = RadarTarget.GetCorrelatedFlightPlan().GetControllerAssignedData().GetSquawk();
		if (strlen(assr) == 4 &&
			atoi(assr) % 100 != 0 &&
			strcmp(assr, squawkModeS) != 0 &&
			strcmp(assr, squawkVFR) != 0)
		{
			usedCodes.push_back(assr);
		}

		// search for all actual codes used by pilots
		auto pssr = RadarTarget.GetPosition().GetSquawk();
		if (strlen(pssr) == 4 &&
			atoi(pssr) % 100 != 0 &&
			strcmp(pssr, squawkModeS) != 0 &&
			strcmp(pssr, squawkVFR) != 0 &&
			strcmp(pssr, assr) != 0)
		{
			usedCodes.push_back(pssr);
		}

		sort(usedCodes.begin(), usedCodes.end());
		usedCodes.erase(unique(usedCodes.begin(), usedCodes.end()), usedCodes.end());
	}
	return usedCodes;
}

string ESversion()
{
	int EuroScopeVersion[4] = { 0, 0, 0, 0 };

	DWORD verHandle = 0;
	char szVersionFile[MAX_PATH];
	GetModuleFileName(NULL, szVersionFile, MAX_PATH);
	const DWORD verSize = GetFileVersionInfoSize(szVersionFile, &verHandle);

	if (verSize != NULL && verHandle == 0)
	{
		char* verData = new char[verSize];

		if (GetFileVersionInfo(szVersionFile, verHandle, verSize, verData))
		{
			VS_FIXEDFILEINFO* verInfo = nullptr;
			unsigned int size = 0;

			if (VerQueryValue(verData, "\\", (LPVOID*)&verInfo, &size))
			{
				if (size)
				{
					if (verInfo->dwSignature == 0xfeef04bd)
					{
						EuroScopeVersion[0] = (verInfo->dwFileVersionMS >> 16) & 0xffff;
						EuroScopeVersion[1] = (verInfo->dwFileVersionMS >> 0) & 0xffff;
						EuroScopeVersion[2] = (verInfo->dwFileVersionLS >> 16) & 0xffff;
						EuroScopeVersion[3] = (verInfo->dwFileVersionLS >> 0) & 0xffff;
					}
				}
			}
		}
		delete[] verData;
	}

	return to_string(EuroScopeVersion[0]) + "." + to_string(EuroScopeVersion[1]) + "." + to_string(EuroScopeVersion[2]) + "." + to_string(EuroScopeVersion[3]);
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
