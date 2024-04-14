// Copyright (c) 2020-2024 v0lt, Aleksoid
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "stdafx.h"
#include <sstream>
#include "StringUtil.h"

void str_split(const CStdStringA& str, std::vector<CStdStringA>& tokens, char delim)
{
	std::istringstream iss(str);
	CStdStringA tmp;
	while (std::getline(iss, tmp, delim)) {
		if (tmp.size()) {
			tokens.push_back(tmp);
		}
	}
}

void str_split(const CStdStringW& wstr, std::vector<CStdStringW>& tokens, wchar_t delim)
{
	std::wistringstream iss(wstr);
	CStdStringW tmp;
	while (std::getline(iss, tmp, delim)) {
		if (tmp.size()) {
			tokens.push_back(tmp);
		}
	}
}

void str_replace(CStdStringA& s, const CStdStringA from, const CStdStringA to)
{
	CStdStringA str;
	size_t pos = 0;
	size_t pf = 0;
	while ((pf = s.find(from, pos)) < s.size()) {
		str.append(s, pos, pf - pos);
		str.append(to);
		pos = pf + from.size();
	}
	if (str.size()) {
		str.append(s, pos);
		s = str;
	}
}

void str_replace(CStdStringW& s, const CStdStringW from, const CStdStringW to)
{
	CStdStringW str;
	size_t pos = 0;
	size_t pf = 0;
	while ((pf = s.find(from, pos)) < s.size()) {
		str.append(s, pos, pf - pos);
		str.append(to);
		pos = pf + from.size();
	}
	if (str.size()) {
		str.append(s, pos);
		s = str;
	}
}

CStdStringA ConvertWideToANSI(const CStdStringW& wstr)
{

	int count = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.length(), nullptr, 0, nullptr, nullptr);
	CStdStringA str(count, 0);
	WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &str[0], count, nullptr, nullptr);
	return str;
}

CStdStringW ConvertAnsiToWide(const char* pstr, int size)
{
	int count = MultiByteToWideChar(CP_ACP, 0, pstr, size, nullptr, 0);
	CStdStringW wstr(count, 0);
	MultiByteToWideChar(CP_ACP, 0, pstr, size, &wstr[0], count);
	return wstr;
}

CStdStringW ConvertAnsiToWide(const CStdStringA& str)
{
	int count = MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.length(), nullptr, 0);
	CStdStringW wstr(count, 0);
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.length(), &wstr[0], count);
	return wstr;
}

CStdStringA ConvertWideToUtf8(const CStdStringW& wstr)
{
	int count = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), nullptr, 0, nullptr, nullptr);
	CStdStringA str(count, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], count, nullptr, nullptr);
	return str;
}

CStdStringW ConvertUtf8ToWide(const CStdStringA& str)
{
	int count = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), nullptr, 0);
	CStdStringW wstr(count, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), &wstr[0], count);
	return wstr;
}

CStdStringW ConvertUtf8ToWide(const char* pstr, int size)
{
	int count = MultiByteToWideChar(CP_UTF8, 0, pstr, size, nullptr, 0);
	CStdStringW wstr(count, 0);
	MultiByteToWideChar(CP_UTF8, 0, pstr, size, &wstr[0], count);
	return wstr;
}
