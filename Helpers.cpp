#include "stdafx.h"
#include "Helpers.h"
#include "version.h"

#ifdef USE_HTTPLIB
/* NEW HTTPLIB Client implementation */
string LoadUpdateString()
{
	httplib::Client cli(MY_PLUGIN_UPDATE_BASE);
	auto res = cli.Get(MY_PLUGIN_UPDATE_ENDPOINT);

	if (!res) {
		throw std::runtime_error("Connection failed to verify the plugin version. Error: " + httplib::to_string(res.error()));
	}

	if (res->status != httplib::StatusCode::OK_200) {
		throw std::runtime_error("Failed to verify the plugin version. HTTP status: " + std::to_string(res->status));
	}

	return res->body;
}

// Function that performs async HTTP GET and returns a future
std::future<std::string> async_http_get(const std::string& host, const std::string& path) {
	std::promise<std::string> body_promise;
	auto future = body_promise.get_future();

	// Shared client to keep alive
	auto cli = std::make_shared<httplib::Client>(host.c_str());

	// Capture both client and promise
	std::thread([cli, path, promise = std::move(body_promise)]() mutable {
		auto res = cli->Get(path.c_str());
		if (res && res->status == 200) {
			promise.set_value(res->body);
		}
		else {
			throw std::runtime_error("Connection failed.\nHTTP status: " + std::to_string(res->status) + "Error: " + httplib::to_string(res.error()));
			promise.set_value("");
		}
		}).detach();

	return future;
}
#else
string LoadUpdateString()
{
	const string AGENT{ "EuroScope " + (string)EuroScopeVersion() + " plug-in: " + MY_PLUGIN_NAME + "/" + VER_FILEVERSION_STR};
	HINTERNET connect = InternetOpen(AGENT.c_str(), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (!connect) {
		throw error{ string {"Connection failed to verify the plugin version. Error: " + to_string(GetLastError()) } };
	}

	HINTERNET OpenAddress = InternetOpenUrl(connect, MY_PLUGIN_UPDATE_URL, NULL, 0, INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD, 0);
	if (!OpenAddress) {
		InternetCloseHandle(connect);
		throw error{ string { "Failed to load plugin version verification file. Error: " + to_string(GetLastError()) } };
	}

	char DataReceived[256];
	DWORD NumberOfBytesRead{ 0 };
	string answer;
	while (InternetReadFile(OpenAddress, DataReceived, 256, &NumberOfBytesRead) && NumberOfBytesRead)
		answer.append(DataReceived, NumberOfBytesRead);

	InternetCloseHandle(OpenAddress);
	InternetCloseHandle(connect);
	return answer;
}
#endif

#ifdef USE_HTTPLIB
/* NEW HTTPLIB Client implementation */
#ifdef _DEBUG
string LoadWebSquawkO(CCAMS& ccams, CFlightPlan& FlightPlan)
{
	string codes;
	for (size_t i = 0; i < ccams.collectUsedCodes(FlightPlan).size(); i++)
	{
		if (i > 0)
			codes += "~";
		codes += ccams.collectUsedCodes(FlightPlan)[i];
	}

	string query_string = "callsign=" + string(ccams.ControllerMyself().GetCallsign());
	if (FlightPlan.IsValid())
	{
		if (ccams.IsADEPvicinity(FlightPlan))
		{
			query_string += "&orig=" + string(FlightPlan.GetFlightPlanData().GetOrigin());
		}
		query_string += "&dest=" + string(FlightPlan.GetFlightPlanData().GetDestination()) +
			"&flightrule=" + string(FlightPlan.GetFlightPlanData().GetPlanType());

		if (FlightPlan.GetCorrelatedRadarTarget().IsValid())
			if (FlightPlan.GetCorrelatedRadarTarget().GetPosition().IsValid())
				query_string += "&latitude=" + to_string(FlightPlan.GetCorrelatedRadarTarget().GetPosition().GetPosition().m_Latitude) +
					"&longitude=" + to_string(FlightPlan.GetCorrelatedRadarTarget().GetPosition().GetPosition().m_Longitude);

		query_string += "&connectiontype=" + to_string(ccams.GetConnectionType());

#ifndef _DEBUG
		if (ccams.GetConnectionType() > 2)
		{
			query_string += "&sim";
		}
#endif
	}
	if (!codes.empty())
	{
		query_string += "&codes=" + codes;
	}
	httplib::Headers headers = {
		{"User-Agent", "EuroScope " + (string)EuroScopeVersion() + " plug-in: " + MY_PLUGIN_NAME + "/" + VER_FILEVERSION_STR}
	};

	httplib::Client client(MY_PLUGIN_APP_BASE);
	string uri = MY_PLUGIN_APP_ENDPOINT + string("?") + query_string;
	auto res = client.Get(uri, headers);

	if (!res || res->status != httplib::StatusCode::OK_200) {
		return string{ "E" + httplib::to_string(res.error())};
	}

	string answer = res->body;
	trim(answer);

	if (answer.empty())
		return string{ "E411" };
	return answer;
}
#endif

string LoadWebSquawk(const CFlightPlan& FlightPlan, const CController& ATCO, vector<string> usedCodes, bool vicinityADEP, const int ConnectionType)
{
	string codes;
	for (size_t i = 0; i < usedCodes.size(); i++)
	{
		if (i > 0)
			codes += "~";
		codes += usedCodes[i];
	}

	string query_string = "callsign=" + string(ATCO.GetCallsign());
	if (FlightPlan.IsValid())
	{
		if (vicinityADEP)
		{
			query_string += "&orig=" + string(FlightPlan.GetFlightPlanData().GetOrigin());
		}
		query_string += "&dest=" + string(FlightPlan.GetFlightPlanData().GetDestination()) +
			"&flightrule=" + string(FlightPlan.GetFlightPlanData().GetPlanType());

		if (FlightPlan.GetCorrelatedRadarTarget().IsValid())
			if (FlightPlan.GetCorrelatedRadarTarget().GetPosition().IsValid())
				query_string += "&latitude=" + to_string(FlightPlan.GetCorrelatedRadarTarget().GetPosition().GetPosition().m_Latitude) +
				"&longitude=" + to_string(FlightPlan.GetCorrelatedRadarTarget().GetPosition().GetPosition().m_Longitude);

		query_string += "&connectiontype=" + to_string(ConnectionType);

#ifndef _DEBUG
		if (ConnectionType > 2)
		{
			query_string += "&sim";
		}
#endif
	}
	if (!codes.empty())
	{
		query_string += "&codes=" + codes;
	}
	httplib::Headers headers = {
		{"User-Agent", "EuroScope " + (string)EuroScopeVersion() + " plug-in: " + MY_PLUGIN_NAME + "/" + VER_FILEVERSION_STR}
	};

	httplib::Client client(MY_PLUGIN_APP_BASE);
	string uri = MY_PLUGIN_APP_ENDPOINT + string("?") + query_string;
	auto res = client.Get(uri, headers);

	if (!res || res->status != httplib::StatusCode::OK_200) {
		return string{ httplib::to_string(res.error()) };
	}

	string answer = res->body;
	trim(answer);

	if (answer.empty())
		return string{ "E411" };
	return answer;
}
#else
string LoadWebSquawk(CCAMS& ccams, CFlightPlan& FlightPlan)
{
	//PluginData p;
	//const string AGENT{ "EuroScope " + string { MY_PLUGIN_NAME } + "/" + string { VER_FILEVERSION_STR } };
	const string AGENT{ "EuroScope " + (string)EuroScopeVersion() + " plug-in: " + MY_PLUGIN_NAME + "/" + VER_FILEVERSION_STR};
	HINTERNET connect = InternetOpen(AGENT.c_str(), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (!connect) {
#ifdef _DEBUG
		//throw error { string { "Failed reach the CCAMS server. Error: " + to_string(GetLastError()) } };
#endif
		return string{ "E404" };
	}

	//int statusCode;
	char responseText[MAX_PATH]; // change to wchar_t for unicode
	DWORD responseTextSize = sizeof(responseText);

	//Check existance of page (for 404 error)
	if (HttpQueryInfo(connect, HTTP_QUERY_STATUS_CODE, &responseText, &responseTextSize, NULL))
	{
		//statusCode = ;
		if (atoi(responseText) != 200)
			return string{ "E" + string(responseText) };
	}

	//cpr::Response r = cpr::Get(cpr::Url{ "http://localhost/webtools/CCAMS/squawk" },
	//	cpr::Parameters{ {"callsign", ccams.ControllerMyself().GetCallsign()} });
	//if (r.status_code != 200)
	//	return "CURL ERROR";


	string codes;
	for (size_t i = 0; i < ccams.collectUsedCodes(FlightPlan).size(); i++)
	{
		if (i > 0)
			codes += ",";
		codes += ccams.collectUsedCodes(FlightPlan)[i];
	}

	//string build_url = "http://localhost/webtools/CCAMS/squawk?callsign=" + string(ccams.ControllerMyself().GetCallsign());
	string build_url = "https://ccams.kilojuliett.ch/squawk?callsign=" + string(ccams.ControllerMyself().GetCallsign());
	if (FlightPlan.IsValid())
	{
		if (ccams.IsADEPvicinity(FlightPlan))
		{
			build_url += "&orig=" + string(FlightPlan.GetFlightPlanData().GetOrigin());
		}
		build_url += "&dest=" + string(FlightPlan.GetFlightPlanData().GetDestination()) +
			"&flightrule=" + string(FlightPlan.GetFlightPlanData().GetPlanType());

		if (FlightPlan.GetCorrelatedRadarTarget().IsValid())
			if (FlightPlan.GetCorrelatedRadarTarget().GetPosition().IsValid())
				build_url += "&latitude=" + to_string(FlightPlan.GetCorrelatedRadarTarget().GetPosition().GetPosition().m_Latitude) +
				"&longitude=" + to_string(FlightPlan.GetCorrelatedRadarTarget().GetPosition().GetPosition().m_Longitude);

		build_url += "&connectiontype=" + to_string(ccams.GetConnectionType());

#ifndef _DEBUG
		if (ccams.GetConnectionType() > 2)
		{
			build_url += "&sim";
		}
#endif
	}
	if (codes.size() > 0)
	{
		build_url += "&codes=" + codes;
	}

	HINTERNET OpenAddress = InternetOpenUrl(connect, build_url.c_str(), NULL, 0, INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD, 0);
	if (!OpenAddress) {
		InternetCloseHandle(connect);
#ifdef _DEBUG
		//throw error{ string { "Failed reach the CCAMS server. Error: " + to_string(GetLastError()) } };
#endif

		return string{ "E406" };
	}

	char DataReceived[256];
	DWORD NumberOfBytesRead{ 0 };
	string answer;
	while (InternetReadFile(OpenAddress, DataReceived, 256, &NumberOfBytesRead) && NumberOfBytesRead)
		answer.append(DataReceived, NumberOfBytesRead);

	InternetCloseHandle(OpenAddress);
	InternetCloseHandle(connect);

	trim(answer);

	if (answer.length() == 0)
		return string{ "E411" };

	return answer;
}
#endif

std::vector<int> GetExeVersion() {
	// Get the path of the executable
	char exePath[MAX_PATH];
	if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0) {
		return {}; // Return an empty vector on failure
	}

	// Get the size of the version info resource
	DWORD handle = 0; // Explicitly initialize handle
	DWORD versionInfoSize = GetFileVersionInfoSizeA(exePath, &handle);
	if (versionInfoSize == 0) {
		return {}; // Return an empty vector on failure
	}

	// Allocate memory to hold the version info
	std::vector<char> versionInfo(versionInfoSize);
	if (!GetFileVersionInfoA(exePath, handle, versionInfoSize, versionInfo.data())) {
		return {}; // Return an empty vector on failure
	}

	// Extract the fixed file info
	VS_FIXEDFILEINFO* fileInfo = nullptr;
	UINT fileInfoSize = 0;
	if (!VerQueryValueA(versionInfo.data(), "\\", reinterpret_cast<LPVOID*>(&fileInfo), &fileInfoSize)) {
		return {}; // Return an empty vector on failure
	}

	if (fileInfo) {
		// Extract version information and return as an array
		return {
			static_cast<int>(HIWORD(fileInfo->dwFileVersionMS)), // Major
			static_cast<int>(LOWORD(fileInfo->dwFileVersionMS)), // Minor
			static_cast<int>(HIWORD(fileInfo->dwFileVersionLS)), // Build
			static_cast<int>(LOWORD(fileInfo->dwFileVersionLS))  // Revision
		};
	}

	return {}; // Return an empty vector if no version info is available
}

std::vector<int> parseVersion(const std::string& version) {
    std::vector<int> parts;
    std::stringstream ss(version);
    std::string item;

    while (std::getline(ss, item, ',')) {
        parts.push_back(std::stoi(item));
    }

    // Ensure the version has exactly 4 parts
    while (parts.size() < 4) {
        parts.push_back(0);
    }

    return parts;
}

int compareVersions(const std::vector<int>& v1, const std::vector<int>& v2) {
	for (size_t i = 0; i < 4; ++i) {
		if (v1[i] < v2[i]) return -1;
		if (v1[i] > v2[i]) return 1;
	}
	return 0; // equal
}

string EuroScopeVersion()
{
	vector<int> version = GetExeVersion();
	if (version.size() > 3)
		return to_string(version[0]) + "." + to_string(version[1]) + "." + to_string(version[2]) + "." + to_string(version[3]);

	return "{NO VERSION DATA}";
}

string GetCompiledVersion() {
	ostringstream ss;
	ss << VER_MAJOR << "." << VER_MINOR << "." << VER_PATCH << "." << VER_BUILD;
	return ss.str();
}