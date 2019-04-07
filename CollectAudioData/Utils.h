#pragma once

#include <string>

class Utils
{
public:
	Utils();
	~Utils();
public:
	static std::wstring string2wstring(std::string str);
	static std::string wstring2string(std::wstring wstr);
};

