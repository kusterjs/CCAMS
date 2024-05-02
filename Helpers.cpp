#include "stdafx.h"
#include "Helpers.h"

string LoadUpdateString()
{
	const string AGENT{ "EuroScope " + (string)ESversion() + " plug-in: " + MY_PLUGIN_NAME + "/" + MY_PLUGIN_VERSION};

	/* NEW HTTPLIB Client implementation */
	httplib::Client cli(MY_PLUGIN_UPDATE_BASE);
	auto res = cli.Get(MY_PLUGIN_UPDATE_ENDPOINT);

	if (!res || res->status != httplib::StatusCode::OK_200) {
		auto err = res.error();
		throw error{ string {"Connection failed to verify the plugin version. Error: " + httplib::to_string(err) } };
	}

	std::string answer = res->body;

	return answer;
}

string LoadWebSquawk(EuroScopePlugIn::CFlightPlan FP, EuroScopePlugIn::CController ATCO, vector<const char*> usedCodes, bool vicinityADEP, int ConnectionType)
{
	string codes;
	for (size_t i = 0; i < usedCodes.size(); i++)
	{
		if (i > 0)
			codes += ",";
		codes += usedCodes[i];
	}

	string query_string = "callsign=" + string(ATCO.GetCallsign());
	if (FP.IsValid())
	{
		if (vicinityADEP)
		{
			query_string += "&orig=" + string(FP.GetFlightPlanData().GetOrigin());
		}
		query_string += "&dest=" + string(FP.GetFlightPlanData().GetDestination()) +
			"&flightrule=" + string(FP.GetFlightPlanData().GetPlanType());

		if (FP.GetCorrelatedRadarTarget().IsValid())
			if (FP.GetCorrelatedRadarTarget().GetPosition().IsValid())
				query_string += "&latitude=" + to_string(FP.GetCorrelatedRadarTarget().GetPosition().GetPosition().m_Latitude) +
					"&longitude=" + to_string(FP.GetCorrelatedRadarTarget().GetPosition().GetPosition().m_Longitude);

		query_string += "&connectiontype=" + to_string(ConnectionType);

#ifndef _DEBUG
		if (ConnectionType > 2)
		{
			query_string += "&sim";
		}
#endif
	}
	if (codes.size() > 0)
	{
		query_string += "&codes=" + codes;
	}

	httplib::Headers headers = {
		{"User-Agent", "EuroScope " + (string)ESversion() + " plug-in: " + MY_PLUGIN_NAME + "/" + MY_PLUGIN_VERSION }
	};

	httplib::Client client(MY_PLUGIN_APP_BASE);
	std::string uri = MY_PLUGIN_APP_ENDPOINT + std::string("?") + query_string;
	auto res = client.Get(uri, headers);

	if (!res || res->status != httplib::StatusCode::OK_200) {
		auto err = res.error();
		return string{ httplib::to_string(err) };
	}

	std::string answer = res->body;

	trim(answer);

	if (answer.length() == 0)
		return string{ "E411" };

	return answer;
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