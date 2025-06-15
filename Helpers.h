#pragma once
#include <vector>
#include <string>
#include <sstream>
#include "CCAMS.h"

#ifdef USE_HTTPLIB
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <Windows.h>
#else
#include <WinInet.h>
#endif

using namespace std;
using namespace EuroScopePlugIn;

#ifdef _DEBUG
#ifndef EXTERNAL_FUNCTION_H
#define EXTERNAL_FUNCTION_H

class CCAMS;
string LoadWebSquawkO(CCAMS& ccams, CFlightPlan& FlightPlan);

#endif // EXTERNAL_FUNCTION_H
#endif // _DEBUG


string LoadWebSquawk(const CFlightPlan& FlightPlan, const CController& ATCO, vector<string> usedCodes, bool vicinityADEP, const int ConnectionType);
string LoadUpdateString();
std::future<std::string> async_http_get(const std::string& host, const std::string& path);

vector<int> GetExeVersion();
std::vector<int> parseVersion(const std::string& version);
int compareVersions(const std::vector<int>& v1, const std::vector<int>& v2);

string EuroScopeVersion();
string GetCompiledVersion();

inline vector<string> split(const string & s, char delim)
{
	istringstream ss(s);
	string item;
	vector<string> elems;

	while (getline(ss, item, delim))
		elems.push_back(item);
	return elems;
}

// trim from start (in place)
static inline void ltrim(string& s) {
	s.erase(s.begin(), find_if(s.begin(), s.end(), [](unsigned char ch) {
		return !isspace(ch);
		}));
}

// trim from end (in place)
static inline void rtrim(string& s) {
	s.erase(find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
		return !isspace(ch);
		}).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(string& s) {
	ltrim(s);
	rtrim(s);
}

class modesexception
	: public exception
{
public:
	explicit modesexception(string & what) : exception { what.c_str() } {}
	virtual inline const long icon() const = 0;
	inline void whatMessageBox()
	{
		MessageBox(NULL, what(), "CCAMS", MB_OK | icon());
	}
};

class error
	: public modesexception
{
public:
	explicit error(string && what) : modesexception { what } {}
	inline const long icon() const
	{
		return MB_ICONERROR;
	}
};

class warning
	: public modesexception
{
public:
	explicit warning(string && what) : modesexception { what } {}
	inline const long icon() const
	{
		return MB_ICONWARNING;
	}
};
