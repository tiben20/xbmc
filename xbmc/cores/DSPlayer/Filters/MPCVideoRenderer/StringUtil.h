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

#pragma once
#include "utils/StdString.h"
#include <cctype>
#include <locale>

//
// convert string to lower or upper case
//

inline void str_tolower(CStdStringA& s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); } );
	// char for std::tolower should be converted to unsigned char
}

inline void str_toupper(CStdStringA& s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); } );
	// char for std::toupper should be converted to unsigned char
}

inline void str_tolower(CStdStringW& s)
{
	std::transform(s.begin(), s.end(), s.begin(), ::tolower);
}

inline void str_toupper(CStdStringW& s)
{
	std::transform(s.begin(), s.end(), s.begin(), ::toupper);
}

inline void str_tolower_all(CStdStringW& s)
{
	const std::ctype<wchar_t>& f = std::use_facet<std::ctype<wchar_t>>(std::locale());
	std::ignore = f.tolower(&s[0], &s[0] + s.size());
}

inline void str_toupper_all(CStdStringW& s)
{
	const std::ctype<wchar_t>& f = std::use_facet<std::ctype<wchar_t>>(std::locale());
	std::ignore = f.toupper(&s[0], &s[0] + s.size());
}

//
// split a string using char delimiter
//

void str_split(const CStdStringA& str, std::vector<CStdStringA>& tokens, char delim);

void str_split(const CStdStringW& wstr, std::vector<CStdStringW>& tokens, wchar_t delim);

//
// trimming whitespace
//

/*inline CStdStringA str_trim(const CStdStringA sv)
{
	auto sfront = std::find_if_not(sv.begin(), sv.end(), [](int c) {return isspace(c); });
	auto sback = std::find_if_not(sv.rbegin(), sv.rend(), [](int c) {return isspace(c); }).base();
	return (sback <= sfront ? CStdStringA() : CStdStringA(sfront, sback));
}

inline CStdStringW str_trim(const CStdStringW sv)
{
	auto sfront = std::find_if_not(sv.begin(), sv.end(), [](int c) {return iswspace(c); });
	auto sback = std::find_if_not(sv.rbegin(), sv.rend(), [](int c) {return iswspace(c); }).base();
	return (sback <= sfront ? CStdStringW() : CStdStringW(sfront, sback));
}*/

//
// trimming a character at the end
//

inline void str_trim_end(CStdStringA& s, const char ch)
{
	s.erase(s.find_last_not_of(ch) + 1);
}

inline void str_trim_end(CStdStringW& s, const wchar_t ch)
{
	s.erase(s.find_last_not_of(ch) + 1);
}

//
// truncate after a null character
//

inline void str_truncate_after_null(CStdStringA& s)
{
	s.erase(std::find(s.begin(), s.end(), '\0'), s.end());
}

inline void str_truncate_after_null(CStdStringW& s)
{
	s.erase(std::find(s.begin(), s.end(), '\0'), s.end());
}

//
// replace substring
//

void str_replace(CStdStringA& s, const CStdStringA from, const CStdStringA to);
void str_replace(CStdStringW& s, const CStdStringW from, const CStdStringW to);

//
// simple convert ANSI string to wide character string
//

inline CStdStringW A2WStr(const CStdString sv)
{
	return CStdStringW(sv.begin(), sv.end());
}

//
// converting strings of different formats
//

CStdStringA ConvertWideToANSI(const CStdStringW& wstr);

CStdStringW ConvertAnsiToWide(const CStdStringA& str);
CStdStringW ConvertAnsiToWide(const char* pstr, int size);

CStdStringA ConvertWideToUtf8(const CStdStringW& wstr);

CStdStringW ConvertUtf8ToWide(const CStdStringA& str);
CStdStringW ConvertUtf8ToWide(const char* pstr, int size);
