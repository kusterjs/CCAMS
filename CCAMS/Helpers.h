#pragma once
#include <vector>
#include <string>
#include <sstream>
#include <exception>
#include <WinInet.h>
#include <algorithm> 
#include <cctype>
#include <locale>
#include <EuroScopePlugIn.h>
#include "version.h"
#include "CCAMS.h"


using namespace std;


string LoadUpdateString(PluginData p);

//std::string LoadWebSquawk(std::string origin, std::string callsign, std::vector<const char*> usedCodes);
string LoadWebSquawk(EuroScopePlugIn::CFlightPlan FP, EuroScopePlugIn::CController ATCO, vector<const char*> usedCodes, bool vicinityADEP);


inline std::vector<std::string> split(const std::string & s, char delim)
{
	std::istringstream ss(s);
	std::string item;
	std::vector<std::string> elems;

	while (std::getline(ss, item, delim))
		elems.push_back(item);
	return elems;
}

// trim from start (in place)
static inline void ltrim(std::string& s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
		return !std::isspace(ch);
		}));
}

// trim from end (in place)
static inline void rtrim(std::string& s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
		return !std::isspace(ch);
		}).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string& s) {
	ltrim(s);
	rtrim(s);
}

class modesexception
	: public std::exception
{
public:
	explicit modesexception(std::string & what) : std::exception { what.c_str() } {}
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
	explicit error(std::string && what) : modesexception { what } {}
	inline const long icon() const
	{
		return MB_ICONERROR;
	}
};

class warning
	: public modesexception
{
public:
	explicit warning(std::string && what) : modesexception { what } {}
	inline const long icon() const
	{
		return MB_ICONWARNING;
	}
};
