#pragma once
#include <vector>
#include <string>
#include <regex>
#include <future>
#include <map>
#include "Helpers.h"

using namespace std;
using namespace EuroScopePlugIn;

#define MY_PLUGIN_NAME				PROJECT_NAME
#ifdef USE_HTTPLIB
#define MY_PLUGIN_UPDATE_BASE		"https://raw.githubusercontent.com"
#define MY_PLUGIN_UPDATE_ENDPOINT	"/kusterjs/CCAMS/master/version.txt"
#define MY_PLUGIN_CONFIG_BASE		"https://raw.githubusercontent.com"
#define MY_PLUGIN_CONFIG_ENDPOINT	"/kusterjs/CCAMS/master/config.txt"
#define MY_PLUGIN_APP_BASE			"https://ccams.kilojuliett.ch"
#define MY_PLUGIN_APP_ENDPOINT		"/squawk"
#else
#define MY_PLUGIN_UPDATE_URL	"https://raw.githubusercontent.com/kusterjs/CCAMS/master/config2.txt"
#endif
#define MY_PLUGIN_DEVELOPER			"Jonas Kuster, Pierre Ferran, Oliver Grützmann"
#define MY_PLUGIN_COPYRIGHT			"GPL v3"
//#define MY_PLUGIN_VIEW      "Standard ES radar screen"

struct ItemCodes
{
	enum ItemTypes : int
	{
		TAG_ITEM_ISMODES = 501,
		TAG_ITEM_EHS_HDG,
		TAG_ITEM_EHS_ROLL,
		TAG_ITEM_EHS_GS,
		TAG_ITEM_ERROR_MODES_USE,
		TAG_ITEM_SQUAWK,
		TAG_ITEM_EHS_PINNED
	};

	enum ItemFunctions : int
	{
		TAG_FUNC_SQUAWK_POPUP = 869,
		TAG_FUNC_ASSIGN_SQUAWK,
		TAG_FUNC_ASSIGN_SQUAWK_AUTO,
		TAG_FUNC_ASSIGN_SQUAWK_MANUAL,
		TAG_FUNC_ASSIGN_SQUAWK_VFR,
		TAG_FUNC_ASSIGN_SQUAWK_MODES,
		TAG_FUNC_ASSIGN_SQUAWK_DISCRETE,
		TAG_FUNC_TOGGLE_EHS_LIST
	};
};

struct EquipmentCodes
{
	string FAA{ "HLEGWQS" };
	string ICAO_MODE_S{ "EHILS" };
	string ICAO_EHS{ "EHLS" };
};

struct SquawkCodes
{
	const char* MODE_S{ "1000" };
	const char* VFR{ "7000" };
};

struct ModeS
{
	const regex AIRPORTS_MATCH{ "^((E([BDHLT]|P(?!CE|DA|DE|IR|KS|LK|LY|MB|MI|MM|OK|PR|PW|SN|TM)|URM)|L([DHIKORZ]|F(?!VP)))[A-Z]{2}|LS(A|G[CG]|Z[BGHR]))", regex::icase };
	const regex AIRPORTS_NOTMATCH{ "^EP(CE|DA|DE|IR|KS|LK|LY|MB|MI|MM|OK|PR|PW|SN|TM)|LFVP", regex::icase };
	const regex ROUTE_MATCH{ "", regex::icase };
	const regex ROUTE_NOTMATCH{ "KOSEB|SONAL|SALLO|BAKLI|OKAGA|BIKRU|DETNI|BILRA", regex::icase };
};


class CCAMS :
	public EuroScopePlugIn::CPlugIn
{
public:
	explicit CCAMS(const EquipmentCodes&& ec = EquipmentCodes(), const SquawkCodes&& sc = SquawkCodes(), const ModeS&& ms = ModeS());
	virtual ~CCAMS();

	bool OnCompileCommand(const char* sCommandLine);
	void OnGetTagItem(CFlightPlan FlightPlan, CRadarTarget RadarTarget,
					  int ItemCode,
					  int TagData,
					  char sItemString[16],
					  int * pColorCode,
					  COLORREF * pRGB,
					  double * pFontSize);

	void OnFlightPlanDisconnect(CFlightPlan FlightPlan);
	void OnFlightPlanFlightPlanDataUpdate(CFlightPlan FlightPlan);
	void OnFlightPlanFlightStripPushed(CFlightPlan FlightPlan, const char* sSenderController, const char* sTargetController);

	void OnRefreshFpListContent(CFlightPlanList AcList);
	void OnFunctionCall(int FunctionId,
						const char * sItemString,
						POINT Pt,
						RECT Area);

	void OnTimer(int Counter);

	bool PluginCommands(cmatch Command);

	bool IsADEPvicinity(const CFlightPlan& FlightPlan) const;
	vector<string> collectUsedCodes(const CFlightPlan& FlightPlan);

private:
	future<string> fUpdateString;
	future<string> fVersion;
	future<string> fConfig;
	vector<string> ProcessedFlightPlans;
	vector<string> PendingSquawkRequests;
	map<const char*, future<string>> PendingSquawks;
	regex ModeSAirports;
	regex ModeSAirportsExcl;
	regex ModeSRoute;
	regex ModeSRouteExcl;
	CFlightPlanList FpListEHS;
	vector<string> EHSListFlightPlans;
	string EquipmentCodesFAA;
	string EquipmentCodesICAO;
	string EquipmentCodesICAOEHS;
	const char* squawkModeS;
	const char* squawkVFR;
	int ConnectionState;
	int RemoteConnectionState;
	bool pluginVersionCheck;
	bool acceptEquipmentICAO;
	bool acceptEquipmentFAA;
	bool updateOnStartTracking;
	int autoAssign;
	int APTcodeMaxGS;
	int APTcodeMaxDist;

	void AssignAutoSquawk(CFlightPlan& FlightPlan);
	void AssignSquawk(CFlightPlan& FlightPlan);
	void AssignPendingSquawks();
	void RequestSquawks();
	void CheckVersion(future<string> & message);
	void LoadConfig(future<string>& message);
	void ReadSettings();

	bool IsFlightPlanProcessed(CFlightPlan& FlightPlan);
	bool IsAcModeS(const CFlightPlan& FlightPlan) const;
	bool IsApModeS(const string& icao) const;
	bool IsRteModeS(const CFlightPlan& FlightPlan) const;
	bool IsEHS(const CFlightPlan& FlightPlan) const;
	bool HasEquipment(const CFlightPlan& FlightPlan, bool acceptEquipmentFAA, bool acceptEquipmentICAO, string CodesICAO) const;
	double GetDistanceFromOrigin(const CFlightPlan& FlightPlan) const;
	bool IsEligibleSquawkModeS(const CFlightPlan& FlightPlan) const;
	bool HasValidSquawk(const CFlightPlan& FlightPlan);

	bool HasDuplicateSquawk(const CFlightPlan& FlightPlan);
	bool HasDuplicateSquawk(const CRadarTarget& RadarTarget);
	bool HasDuplicatePSSR(const CFlightPlan& FlightPlan);


#ifdef _DEBUG
	void writeLogFile(stringstream& sText);
#endif // _DEBUG

};
