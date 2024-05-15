/*
 * the shader caching coming from https://github.com/Blinue/Magpie
 *
 * (C) 2022-2024 Ti-BEN
 *
 * This file is part of KODI
 *
 * KODI is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * KODI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
 
 
#include "stdafx.h"
#include "ShadersLoader.h"
#include "ShadersCache.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <mutex>
#include <filesystem>
#include <xutility>
#include <bit>
#include <bitset>
//#include "utility.h"
#include "filesystem/File.h"
#include "filesystem/Directory.h"
#include "corecrt_io.h"
//#include "FileUtility.h"
#include <bitset>
#include <unordered_set>
#include <charconv>
#include "ctype.h"
#include "helper.h"
#include "shaders.h"
#include "utils/CharsetConverter.h"
#include "smallvector.h"
#define HAS_SINGLE_BIT(_Val) ((_Val != 0 && (_Val & (_Val - 1)) == 0))// _Val != 0 && (_Val & (_Val - 1)) == 0;
static constexpr
const char* META_INDICATOR = "//!";

class PassInclude : public ID3DInclude {
public:
	PassInclude(std::wstring_view localDir) : _localDir(localDir) {}

	PassInclude(const PassInclude&) = default;
	PassInclude(PassInclude&&) = default;

	HRESULT CALLBACK Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes)
	{
		XFILE::CFile includeFile;

		std::string fileName("special://xbmc/system/shaders/mpcvr/");
		fileName.append(pFileName);

		if (!includeFile.Open(fileName))
		{
			CLog::LogF(LOGERROR, "Could not open 3DLUT file: {}", fileName);
			return E_FAIL;
		}

		int64_t length = includeFile.GetLength();
		void* pData = malloc(length);
		if (includeFile.Read(pData, length) != length)
		{
			free(pData);
			return E_FAIL;
		}
		*ppData = pData;
		*pBytes = length;

		return S_OK;
	}

	HRESULT CALLBACK Close(LPCVOID pData) noexcept override {
		free((void*)pData);
		return S_OK;
	}

private:
	std::wstring _localDir;
};

static void RunParallel(std::function<void(uint32_t)> func, uint32_t times) noexcept {

	for (UINT i = 0; i < times; ++i) {
		func(i);
	}
}
bool ReadTextFile(const wchar_t* fileName, std::string& result) {
	FILE* hFile;
	if (_wfopen_s(&hFile, fileName, L"rt") || !hFile) {
		return false;
	}
	int fd = _fileno(hFile);
	long size = _filelength(fd);

	result.clear();
	result.resize(static_cast<size_t>(size) + 1, 0);

	size_t readed = fread(result.data(), 1, size, hFile);
	result.resize(readed);

	fclose(hFile);
	return true;
}

static uint64_t _wymix(uint64_t lhs, uint64_t rhs) noexcept {
#ifdef _M_X64
	uint64_t hi;
	uint64_t lo = _umul128(lhs, rhs, &hi);
#elif defined(_M_ARM64)
	uint64_t lo = lhs * rhs;
	uint64_t hi = __umulh(lhs, rhs);
#else
#error "不支持的 CPU 架构"
#endif
	return lo ^ hi;
}

// read functions
static uint64_t _wyr8(const uint8_t* p) noexcept {
	uint64_t v;
	memcpy(&v, p, 8);
	return v;
}

static uint64_t _wyr4(const uint8_t* p) noexcept {
	uint32_t v;
	memcpy(&v, p, 4);
	return v;
}

static uint64_t _wyr3(const uint8_t* p, size_t k) noexcept {
	return (((uint64_t)p[0]) << 16) | (((uint64_t)p[k >> 1]) << 8) | p[k - 1];
}
// the default secret parameters
static const uint64_t _wyp[4] = { 0xa0761d6478bd642full, 0xe7037ed1a0b428dbull, 0x8ebc6af09c88c6e3ull, 0x589965cc75374cc3ull };

uint64_t HashData(std::vector<BYTE> data) noexcept {
	const size_t len = data.size();
	uint64_t seed = _wyp[0];
	
	const uint8_t* p = (const uint8_t*)data.data();
	uint64_t a, b;
	if (len <= 16) {
		if (len >= 4) {
			a = (_wyr4(p) << 32) | _wyr4(p + ((len >> 3) << 2));
			b = (_wyr4(p + len - 4) << 32) | _wyr4(p + len - 4 - ((len >> 3) << 2));
		}
		else if (len > 0) {
			a = _wyr3(p, len);
			b = 0;
		}
		else {
			a = b = 0;
		}
	}
	else {
		size_t i = len;
		if (i > 48) {
			uint64_t see1 = seed, see2 = seed;
			do {
				seed = _wymix(_wyr8(p) ^ _wyp[1], _wyr8(p + 8) ^ seed);
				see1 = _wymix(_wyr8(p + 16) ^ _wyp[2], _wyr8(p + 24) ^ see1);
				see2 = _wymix(_wyr8(p + 32) ^ _wyp[3], _wyr8(p + 40) ^ see2);
				p += 48;
				i -= 48;
			} while (i > 48);
			seed ^= see1 ^ see2;
		}

		while (i > 16) {
			seed = _wymix(_wyr8(p) ^ _wyp[1], _wyr8(p + 8) ^ seed);
			i -= 16;
			p += 16;
		}
		a = _wyr8(p + i - 16);  b = _wyr8(p + i - 8);
	}

	return _wymix(_wyp[1] ^ len, _wymix(a ^ _wyp[1], b ^ seed));
}

static std::wstring HexHash(std::vector<BYTE> data) 
{
	uint64_t hashBytes = HashData(data);

	static wchar_t oct2Hex[16] = {
		L'0',L'1',L'2',L'3',L'4',L'5',L'6',L'7',
		L'8',L'9',L'a',L'b',L'c',L'd',L'e',L'f'
	};

	std::wstring result(16, 0);
	wchar_t* pResult = &result[0];

	BYTE* b = (BYTE*)&hashBytes;
	for (int i = 0; i < 8; ++i) {
		*pResult++ = oct2Hex[(*b >> 4) & 0xf];
		*pResult++ = oct2Hex[*b & 0xf];
		++b;
	}

	return result;
}

std::wstring GetHash(
	std::string_view source,
	const phmap::flat_hash_map<std::wstring, float>* inlineParams
) {
	std::string str;
	str.reserve(source.size() + 256);
	str = source;

	str.append(fmt::format("VERSION:{}\n", SCALERCOMPILE_VERSION));
	if (inlineParams) {
		for (const auto& pair : *inlineParams) {
			str.append(fmt::format("{}:{}\n", CCharsetConverter::UTF16ToUTF8(pair.first), std::lroundf(pair.second * 10000)));
		}
	}
	std::vector<BYTE> hashdata;
	
	
	hashdata.resize(source.size());
	for (size_t xx = 0; xx < source.size(); xx++) hashdata.push_back(source.at(xx));

	return HexHash(hashdata);
}

void Trim(std::string_view& str)
{
	for (int i = 0; i < str.size(); ++i)
	{
		if (!isspace(str[i]))
		{
			str.remove_prefix(i);

			size_t i = str.size() - 1;
			for (; i > 0; --i)
			{
				if (!isspace(str[i]))
				{
					break;
				}
			}

			str.remove_suffix(str.size() - 1 - i);
			return;
		}
	}

	str.remove_prefix(str.size());
}

std::string ToUpper(std::string_view str)
{
	std::string uper_case(str);
	std::locale loc;
	for (char& s : uper_case)
		s = std::toupper(s, loc);
	return uper_case;
}

static std::vector<std::string_view > Split(std::string_view str, char delimiter)
{
	std::vector<std::string_view > result;
	while (!str.empty())
	{
		size_t pos = str.find(delimiter, 0);
		result.push_back(str.substr(0, pos));

		if (pos == std::string_view::npos)
		{
			return result;
		}
		else
		{
			str.remove_prefix(pos + 1);
		}
	}

	return result;
}

static uint32_t RemoveComments(std::string& source) {
	// 确保以换行符结尾
	if (source.back() != '\n') {
		source.push_back('\n');
	}

	std::string result;
	result.reserve(source.size());

	int j = 0;
	// 单独处理最后两个字符
	for (size_t i = 0, end = source.size() - 2; i < end; ++i) {
		if (source[i] == '/') {
			if (source[i + 1] == '/' && source[i + 2] != '!') {
				// 行注释
				i += 2;

				// 无需处理越界，因为必定以换行符结尾
				while (source[i] != '\n') {
					++i;
				}

				// 保留换行符
				source[j++] = '\n';

				continue;
			}
			else if (source[i + 1] == '*') {
				// 块注释
				i += 2;

				while (true) {
					if (++i >= source.size()) {
						// 未闭合
						return 1;
					}

					if (source[i - 1] == '*' && source[i] == '/') {
						break;
					}
				}

				// 文件结尾
				if (i >= source.size() - 2) {
					source.resize(j);
					return 0;
				}

				continue;
			}
		}

		source[j++] = source[i];
	}

	// 无需复制最后的换行符
	source[j++] = source[source.size() - 2];
	source.resize(j);
	return 0;
}

template<bool IncludeNewLine>
static void RemoveLeadingBlanks(std::string_view& source) {
	size_t i = 0;
	for (; i < source.size(); ++i) {
		if constexpr (IncludeNewLine) {
			if (!isspace(source[i])) {
				break;
			}
		}
		else {
			char c = source[i];
			if (c != ' ' && c != '\t') {
				break;
			}
		}
	}

	source.remove_prefix(i);
}

template<bool AllowNewLine>
static bool CheckNextToken(std::string_view& source, std::string_view token) {
	RemoveLeadingBlanks<AllowNewLine>(source);

	if (!source._Starts_with(token)) {
		return false;
	}

	source.remove_prefix(token.size());
	return true;
}

template<bool AllowNewLine>
static uint32_t GetNextToken(std::string_view& source, std::string_view& value) {
	RemoveLeadingBlanks<AllowNewLine>(source);

	if (source.empty()) {
		return 2;
	}

	char cur = source[0];
	if (cur == '\r')
	{
		cur = source[1];
		//handle crappy windows line feed
		
	}
		
	if (isalpha(cur) || cur == '_') {
		size_t j = 1;
		for (; j < source.size(); ++j) {
			cur = source[j];

			if (!isalnum(cur) && cur != '_') {
				break;
			}
		}

		value = source.substr(0, j);
		source.remove_prefix(j);
		return 0;
	}

	if constexpr (AllowNewLine) {
		return 1;
	}
	else {
		
		return cur == '\n' ? 2 : 1;
	}
}

static bool CheckMagic(std::string_view& source) {
	std::string_view token;
	if (!CheckNextToken<true>(source, META_INDICATOR)) {
		return false;
	}

	if (!CheckNextToken<false>(source, "MAGPIE")) {
		return false;
	}
	if (!CheckNextToken<false>(source, "SHADER")) {
		return false;
	}

	if (GetNextToken<false>(source, token) != 2) {
		return false;
	}

	if (source.empty()) {
		return false;
	}

	return true;
}

static uint32_t GetNextString(std::string_view& source, std::string_view& value) {
	RemoveLeadingBlanks<false>(source);
	size_t pos = source.find('\n');

	value = source.substr(0, pos);
	Trim(value);
	if (value.empty()) {
		return 1;
	}

	source.remove_prefix(std::min(pos + 1, source.size()));
	return 0;
}

template<typename T>
static uint32_t GetNextNumber(std::string_view& source, T& value) {
	RemoveLeadingBlanks<false>(source);

	if (source.empty()) {
		return 1;
	}

	const auto& result = std::from_chars(source.data(), source.data() + source.size(), value);
	if ((int)result.ec) {
		return 1;
	}

	// 解析成功
	source.remove_prefix(result.ptr - source.data());
	return 0;
}

static uint32_t GetNextExpr(std::string_view& source, std::string& expr) {
	RemoveLeadingBlanks<false>(source);
	size_t size = std::min(source.find('\n') + 1, source.size());

	// 移除空白字符
	expr.resize(size);

	size_t j = 0;
	for (size_t i = 0; i < size; ++i) {
		char c = source[i];
		if (!isspace(c)) {
			expr[j++] = c;
		}
	}
	expr.resize(j);

	if (expr.empty()) {
		return 1;
	}

	source.remove_prefix(size);
	return 0;
}

static uint32_t ResolveHeader(std::string_view block, ShaderDesc& desc, bool noCompile)
{
	// Options obligatoires : VERSION
	// Options facultatives : USE_DYNAMIC, SORT_NAME

	std::bitset<3> processed;

	std::string_view token;

	while (true) {
		if (!CheckNextToken<true>(block, META_INDICATOR)) {
			break;
		}

		if (GetNextToken<false>(block, token)) 
			return 1;


			std::string t = ToUpper(token);

			if (t == "VERSION") {
				if (processed[0]) {
					return 1;
				}
				processed[0] = true;

				uint32_t version;
				if (GetNextNumber(block, version)) {
					return 1;
				}

				if (version != SCALERCOMPILE_VERSION) {
					return 1;
				}

				if (GetNextToken<false>(block, token) != 2) {
					return 1;
				}
			}
			else if (t == "USE_DYNAMIC") 
			{
				if (processed[1]) {
					return 1;
				}
				processed[1] = true;

				if (GetNextToken<false>(block, token) != 2) {
					return 1;
				}

				desc.flags |= ShaderFlags::UseDynamic;
			}
			else if (t == "SORT_NAME") 
			{
				if (processed[2]) {
					return 1;
				}
				processed[2] = true;

				std::string_view sortName;
				if (GetNextString(block, sortName)) {
					return 1;
				}

				if (noCompile) {
					desc.sortName = sortName;
				}
			}
			else {
				return 1;
			}
		}

		// HEADER block does not contain code part
		if (GetNextToken<true>(block, token) != 2) {
			return 1;
		}

		if (!processed[0]) {
			return 1;
		}

		return 0;
	}


static uint32_t ResolveParameter(std::string_view block, ShaderDesc& desc) {
	// 必需的选项: DEFAULT, MIN, MAX, STEP
	// 可选的选项: LABEL

	std::bitset<5> processed;

	std::string_view token;

	if (!CheckNextToken<true>(block, META_INDICATOR)) {
		return 1;
	}

	if (!CheckNextToken<false>(block, "PARAMETER")) {
		return 1;
	}
	if (GetNextToken<false>(block, token) != 2) {
		return 1;
	}

	ShaderParameterDesc& paramDesc = desc.params.emplace_back();

	std::string_view defaultValue;
	std::string_view minValue;
	std::string_view maxValue;
	std::string_view stepValue;

	while (true) {
		if (!CheckNextToken<true>(block, META_INDICATOR)) {
			break;
		}

		if (GetNextToken<false>(block, token)) {
			return 1;
		}

		std::string t = ToUpper(token);

		if (t == "DEFAULT") {
			if (processed[0]) {
				return 1;
			}
			processed[0] = true;

			if (GetNextString(block, defaultValue)) {
				return 1;
			}
		}
		else if (t == "LABEL") {
			if (processed[1]) {
				return 1;
			}
			processed[1] = true;

			std::string_view label;
			if (GetNextString(block, label)) {
				return 1;
			}
			paramDesc.label = label;
		}
		else if (t == "MIN") {
			if (processed[2]) {
				return 1;
			}
			processed[2] = true;

			if (GetNextString(block, minValue)) {
				return 1;
			}
		}
		else if (t == "MAX") {
			if (processed[3]) {
				return 1;
			}
			processed[3] = true;

			if (GetNextString(block, maxValue)) {
				return 1;
			}
		}
		else if (t == "STEP") {
			if (processed[4]) {
				return 1;
			}
			processed[4] = true;

			if (GetNextString(block, stepValue)) {
				return 1;
			}
		}
		else {
			return 1;
		}
	}

	// 检查必选项
	if (!processed[0] || !processed[2] || !processed[3] || !processed[4]) {
		return 1;
	}

	// 代码部分
	if (GetNextToken<true>(block, token)) {
		return 1;
	}

	if (token == "float") {
		ShaderConstant<float>& constant = paramDesc.constant.emplace<0>();

		if (GetNextNumber(defaultValue, constant.defaultValue)) {
			return 1;
		}
		if (GetNextNumber(minValue, constant.minValue)) {
			return 1;
		}
		if (GetNextNumber(maxValue, constant.maxValue)) {
			return 1;
		}
		if (GetNextNumber(stepValue, constant.step)) {
			return 1;
		}

		if (constant.defaultValue < constant.minValue || constant.maxValue < constant.defaultValue) {
			return 1;
		}
	}
	else if (token == "int") {
		ShaderConstant<int>& constant = paramDesc.constant.emplace<1>();

		if (GetNextNumber(defaultValue, constant.defaultValue)) {
			return 1;
		}
		if (GetNextNumber(minValue, constant.minValue)) {
			return 1;
		}
		if (GetNextNumber(maxValue, constant.maxValue)) {
			return 1;
		}
		if (GetNextNumber(stepValue, constant.step)) {
			return 1;
		}

		if (constant.defaultValue < constant.minValue || constant.maxValue < constant.defaultValue) {
			return 1;
		}
	}
	else {
		return 1;
	}

	if (GetNextToken<true>(block, token)) {
		return 1;
	}
	paramDesc.name = token;

	if (!CheckNextToken<true>(block, ";")) {
		return 1;
	}

	if (GetNextToken<true>(block, token) != 2) {
		return 1;
	}

	return 0;
}


static uint32_t ResolveTexture(std::string_view block, ShaderDesc& desc) {
	// 如果名称为 INPUT 不能有任何选项，含 SOURCE 时不能有任何其他选项
	// 如果名称为 OUTPUT 只能有 WIDTH 或 HEIGHT
	// 否则必需的选项: FORMAT
	// 可选的选项: WIDTH, HEIGHT

	ShaderIntermediateTextureDesc& texDesc = desc.textures.emplace_back();

	std::bitset<4> processed;

	std::string_view token;

	if (!CheckNextToken<true>(block, META_INDICATOR)) {
		return 1;
	}

	if (!CheckNextToken<false>(block, "TEXTURE")) {
		return 1;
	}
	if (GetNextToken<false>(block, token) != 2) {
		return 1;
	}

	while (true) {
		if (!CheckNextToken<true>(block, META_INDICATOR)) {
			break;
		}

		if (GetNextToken<false>(block, token)) {
			return 1;
		}
		
		std::string t = ToUpper(token);

		if (t == "SOURCE") {
			if (processed[0] || processed[2] || processed[3]) {
				return 1;
			}
			processed[0] = true;

			if (GetNextString(block, token)) {
				return 1;
			}

			texDesc.source = token;
		}
		else if (t == "FORMAT") {
			if (processed[1]) {
				return 1;
			}
			processed[1] = true;

			if (GetNextString(block, token)) {
				return 1;
			}

			//using enum ShaderIntermediateTextureFormat;

			static auto formatMap = []() {
				phmap::flat_hash_map<std::string, ShaderIntermediateTextureFormat> result;

				// UNKNOWN 不可用
				constexpr size_t descCount = std::size(FORMAT_DESCS) - 1;
				result.reserve(descCount);
				for (size_t i = 0; i < descCount; ++i) {
					result.emplace(FORMAT_DESCS[i].name, (ShaderIntermediateTextureFormat)i);
				}
				return result;
				}();

				auto it = formatMap.find(std::string(token));
				if (it == formatMap.end()) {
					return 1;
				}

				texDesc.format = it->second;
		}
		else if (t == "WIDTH") {
			if (processed[0] || processed[2]) {
				return 1;
			}
			processed[2] = true;

			if (GetNextExpr(block, texDesc.sizeExpr.first)) {
				return 1;
			}
		}
		else if (t == "HEIGHT") {
			if (processed[0] || processed[3]) {
				return 1;
			}
			processed[3] = true;

			if (GetNextExpr(block, texDesc.sizeExpr.second)) {
				return 1;
			}
		}
		else {
			return 1;
		}
	}

	// WIDTH 和 HEIGHT 必须成对出现
	if (processed[2] != processed[3]) {
		return 1;
	}

	// 代码部分
	if (!CheckNextToken<true>(block, "Texture2D")) {
		return 1;
	}

	if (GetNextToken<true>(block, token)) {
		return 1;
	}

	if (token == desc.textures[0].name) {
		if (processed.any()) {
			return 1;
		}

		// INPUT 已为第一个元素
		desc.textures.pop_back();
	}
	else if (token == desc.textures[1].name) {
		if (processed[0] || processed[1]) {
			return 1;
		}

		// OUTPUT 已为第二个元素
		desc.textures[1].sizeExpr = std::move(texDesc.sizeExpr);
		desc.textures.pop_back();
	}
	else {
		texDesc.name = token;
	}

	if (!CheckNextToken<true>(block, ";")) {
		return 1;
	}

	if (GetNextToken<true>(block, token) != 2) {
		return 1;
	}

	return 0;
}

static uint32_t ResolveSampler(std::string_view block, ShaderDesc& desc) {
	// 必选项: FILTER
	// 可选项: ADDRESS

	ShaderSamplerDesc& samDesc = desc.samplers.emplace_back();

	std::bitset<2> processed;

	std::string_view token;

	if (!CheckNextToken<true>(block, META_INDICATOR)) {
		return 1;
	}

	if (!CheckNextToken<false>(block, "SAMPLER")) {
		return 1;
	}
	if (GetNextToken<false>(block, token) != 2) {
		return 1;
	}

	while (true) {
		if (!CheckNextToken<true>(block, META_INDICATOR)) {
			break;
		}

		if (GetNextToken<false>(block, token)) {
			return 1;
		}

		std::string t = ToUpper(token);

		if (t == "FILTER") {
			if (processed[0]) {
				return 1;
			}
			processed[0] = true;

			if (GetNextString(block, token)) {
				return 1;
			}

			std::string filter = ToUpper(token);

			if (filter == "LINEAR") {
				samDesc.filterType = ShaderSamplerFilterType::Linear;
			}
			else if (filter == "POINT") {
				samDesc.filterType = ShaderSamplerFilterType::Point;
			}
			else {
				return 1;
			}
		}
		else if (t == "ADDRESS") {
			if (processed[1]) {
				return 1;
			}
			processed[1] = true;

			if (GetNextString(block, token)) {
				return 1;
			}

			std::string filter = ToUpper(token);

			if (filter == "CLAMP") {
				samDesc.addressType = ShaderSamplerAddressType::Clamp;
			}
			else if (filter == "WRAP") {
				samDesc.addressType = ShaderSamplerAddressType::Wrap;
			}
			else {
				return 1;
			}
		}
		else {
			return 1;
		}
	}

	if (!processed[0]) {
		return 1;
	}

	// 代码部分
	if (!CheckNextToken<true>(block, "SamplerState")) {
		return 1;
	}

	if (GetNextToken<true>(block, token)) {
		return 1;
	}

	samDesc.name = token;

	if (!CheckNextToken<true>(block, ";")) {
		return 1;
	}

	if (GetNextToken<true>(block, token) != 2) {
		return 1;
	}

	return 0;
}

static uint32_t ResolveCommon(std::string_view& block) {
	// 无选项

	if (!CheckNextToken<true>(block, META_INDICATOR)) {
		return 1;
	}

	if (!CheckNextToken<false>(block, "COMMON")) {
		return 1;
	}

	if (CheckNextToken<true>(block, META_INDICATOR)) {
		return 1;
	}

	return 0;
}

static uint32_t ResolvePasses(
	std::vector<std::string_view>& blocks,
	ShaderDesc& desc
) {
	// 必选项: IN, OUT
	// 可选项: BLOCK_SIZE, NUM_THREADS, STYLE
	// STYLE 为 PS 时不能有 BLOCK_SIZE 或 NUM_THREADS

	std::string_view token;

	// 首先解析通道序号

	// first 为 Pass 序号，second 为在 blocks 中的位置
	SmallVector<std::pair<uint32_t, uint32_t>> passNumbers;
	passNumbers.reserve(blocks.size());

	for (uint32_t i = 0; i < blocks.size(); ++i) {
		if (i == 8)
			i = i;
		std::string_view& block = blocks[i];

		if (!CheckNextToken<true>(block, META_INDICATOR)) {
			return 1;
		}

		if (!CheckNextToken<false>(block, "PASS")) {
			return 1;
		}

		uint32_t index;
		if (GetNextNumber(block, index)) {
			return 1;
		}
		if (GetNextToken<false>(block, token) != 2) {
			return 1;
		}

		passNumbers.emplace_back(index, i);
	}

	// 以通道序号排序
	std::sort(
		passNumbers.begin(),
		passNumbers.end(),
		[](const auto& l, const auto& r) { return l.first < r.first; }
	);

	{
		std::vector<std::string_view> temp = blocks;
		for (uint32_t i = 0; i < blocks.size(); ++i) {
			if (passNumbers[i].first != i + 1) {
				// PASS 序号不连续
				return 1;
			}

			blocks[i] = temp[passNumbers[i].second];
		}
	}

	desc.passes.resize(blocks.size());

	for (uint32_t i = 0; i < blocks.size(); ++i) {
		std::string_view& block = blocks[i];
		auto& passDesc = desc.passes[i];

		// 用于检查输入和输出中重复的纹理
		phmap::flat_hash_map<std::string_view, uint32_t> texNames;
		texNames.reserve(desc.textures.size());
		for (uint32_t j = 0; j < desc.textures.size(); ++j) {
			texNames.emplace(desc.textures[j].name, j);
		}

		std::bitset<6> processed;

		while (true) {
			if (!CheckNextToken<true>(block, META_INDICATOR)) {
				break;
			}

			if (GetNextToken<false>(block, token)) {
				return 1;
			}

			std::string t = ToUpper(token);

			if (t == "IN") {
				if (processed[0]) {
					return 1;
				}
				processed[0] = true;

				std::string_view inputsStr;
				if (GetNextString(block, inputsStr)) {
					return 1;
				}

				for (std::string_view& input : Split(inputsStr, ',')) {
					Trim(input);

					auto it = texNames.find(input);
					if (it == texNames.end() || it->second == 1) {
						// OUTPUT
						return 1;
					}

					passDesc.inputs.push_back(it->second);
					texNames.erase(it);
				}
			}
			else if (t == "OUT") {
				if (processed[1]) {
					return 1;
				}
				processed[1] = true;

				std::string_view outputsStr;
				if (GetNextString(block, outputsStr)) {
					return 1;
				}

				if (i == blocks.size() - 1) {
					//OUTPUT
					if (outputsStr != desc.textures[1].name) {
						return 1;
					}

					passDesc.outputs.push_back(1);
				}
				else {
					std::vector <std::string_view> outputs = Split(outputsStr, ',');
					if (outputs.size() > 8) {
						// 最多 8 个输出
						return 1;
					}

					for (std::string_view& output : outputs) {
						Trim(output);

						auto it = texNames.find(output);
						if (it == texNames.end()) {
							return 1;
						}

						if (it->second == 0 || !desc.textures[it->second].source.empty()) {
							// INPUT
							return 1;
						}

						passDesc.outputs.push_back(it->second);
						texNames.erase(it);
					}
				}
			}
			else if (t == "BLOCK_SIZE") {
				if (processed[2]) {
					return 1;
				}
				processed[2] = true;

				std::string_view val;
				if (GetNextString(block, val)) {
					return 1;
				}

				std::vector<std::string_view> split = Split(val, ',');
				if (split.size() > 2) {
					return 1;
				}

				uint32_t num;
				if (GetNextNumber(split[0], num) || num == 0) {
					return 1;
				}

				if (GetNextToken<false>(split[0], token) != 2) {
					return false;
				}

				passDesc.blockSize.first = num;

				// 如果只有一个数字，则它同时指定长和高
				if (split.size() == 2) {
					if (GetNextNumber(split[1], num) || num == 0) {
						return 1;
					}

					if (GetNextToken<false>(split[1], token) != 2) {
						return false;
					}
				}

				passDesc.blockSize.second = num;
			}
			else if (t == "NUM_THREADS") {
				if (processed[3]) {
					return 1;
				}
				processed[3] = true;

				std::string_view val;
				if (GetNextString(block, val)) {
					return 1;
				}

				std::vector<std::string_view> split = Split(val, ',');
				if (split.size() > 3) {
					return 1;
				}

				for (uint32_t j = 0; j < 3; ++j) {
					uint32_t num = 1;
					if (split.size() > j) {
						if (GetNextNumber(split[j], num)) {
							return 1;
						}

						if (GetNextToken<false>(split[j], token) != 2) {
							return false;
						}
					}

					passDesc.numThreads[j] = num;
				}
			}
			else if (t == "STYLE") {
				if (processed[4]) {
					return 1;
				}
				processed[4] = true;

				std::string_view val;
				if (GetNextString(block, val)) {
					return 1;
				}

				if (val == "PS") {
					passDesc.isPSStyle = true;
					passDesc.blockSize.first = 16;
					passDesc.blockSize.second = 16;
					passDesc.numThreads = { 64,1,1 };
				}
				else if (val != "CS") {
					return 1;
				}
			}
			else if (t == "DESC") {
				if (processed[5]) {
					return 1;
				}
				processed[5] = true;

				std::string_view val;
				if (GetNextString(block, val)) {
					return 1;
				}

				Trim(val);
				passDesc.desc = val;
			}
			else {
				return 1;
			}
		}

		// 必须指定 IN 和 OUT
		if (!processed[0] || !processed[1]) {
			return 1;
		}

		if (passDesc.isPSStyle) {
			if (processed[2] || processed[3]) {
				return 1;
			}
		}
		else {
			if (!processed[2] || !processed[3]) {
				return 1;
			}
		}

		if (passDesc.desc.empty()) {
			passDesc.desc = fmt::format("Pass {}", i + 1);
		}
	}

	return 0;
}

static uint32_t GeneratePassSource(
	const ShaderDesc& desc,
	uint32_t passIdx,
	std::string_view cbHlsl,
	const SmallVector<std::string_view>& commonBlocks,
	std::string_view passBlock,
	const phmap::flat_hash_map<std::wstring, float>* inlineParams,
	std::string& result,
	std::vector<std::pair<std::string, std::string>>& macros
) {
	bool isInlineParams = desc.flags & ShaderFlags::InlineParams;

	const ShaderPassDesc& passDesc = desc.passes[(size_t)passIdx - 1];

	{
		// 估算需要的空间
		size_t reservedSize = 2048 + cbHlsl.size() + passBlock.size();
		for (std::string_view commonBlock : commonBlocks) {
			reservedSize += commonBlock.size();
		}

		result.reserve(reservedSize);
	}

	// 常量缓冲区
	result.append(cbHlsl);

	////////////////////////////////////////////////////////////////////////////////////////////////////////
	//
	// SRV、UAV 和采样器
	// 
	////////////////////////////////////////////////////////////////////////////////////////////////////////

	// SRV
	for (int i = 0; i < passDesc.inputs.size(); ++i) {
		auto& texDesc = desc.textures[passDesc.inputs[i]];
		result.append(fmt::format("Texture2D<{}> {} : register(t{});\n", FORMAT_DESCS[(uint32_t)texDesc.format].srvTexelType, texDesc.name, i));
	}

	// UAV
	for (int i = 0; i < passDesc.outputs.size(); ++i) {
		auto& texDesc = desc.textures[passDesc.outputs[i]];
		result.append(fmt::format("RWTexture2D<{}> {} : register(u{});\n", FORMAT_DESCS[(uint32_t)texDesc.format].uavTexelType, texDesc.name, i));
	}


	if (!desc.samplers.empty()) {
		// 采样器
		for (int i = 0; i < desc.samplers.size(); ++i) {
			result.append(fmt::format("SamplerState {} : register(s{});\n", desc.samplers[i].name, i));
		}
	}

	result.push_back('\n');

	////////////////////////////////////////////////////////////////////////////////////////////////////////
	//
	// 内置宏
	// 
	////////////////////////////////////////////////////////////////////////////////////////////////////////
	macros.reserve(64);
	macros.emplace_back("MP_BLOCK_WIDTH", std::to_string(passDesc.blockSize.first));
	macros.emplace_back("MP_BLOCK_HEIGHT", std::to_string(passDesc.blockSize.second));
	macros.emplace_back("MP_NUM_THREADS_X", std::to_string(passDesc.numThreads[0]));
	macros.emplace_back("MP_NUM_THREADS_Y", std::to_string(passDesc.numThreads[1]));
	macros.emplace_back("MP_NUM_THREADS_Z", std::to_string(passDesc.numThreads[2]));

	if (passDesc.isPSStyle) {
		macros.emplace_back("MP_PS_STYLE", "");
	}

	if (isInlineParams) {
		macros.emplace_back("MP_INLINE_PARAMS", "");
	}

#ifdef _DEBUG
	macros.emplace_back("MP_DEBUG", "");
#endif

	// 用于在 FP32 和 FP16 间切换的宏
	static const char* numbers[] = { "1","2","3","4" };
	if (desc.flags & ShaderFlags::FP16) {
		macros.emplace_back("MP_FP16", "");
		macros.emplace_back("MF", "min16float");

		for (uint32_t i = 0; i < 4; ++i) {
			CStdStringA str1,str2;
			str1 = "MF";
			str1.append(numbers[i]);
			str2 = "min16float";
			str2.append(numbers[i]);
			macros.emplace_back(str1, str2);
			//macros.emplace_back(Concat("MF", numbers[i]), StrUtils::Concat("min16float", numbers[i]));

			for (uint32_t j = 0; j < 4; ++j) {
				str1 = "MF";
				str1.append(numbers[i]).append("x").append(numbers[j]);
				str2 = "min16float";
				str2.append(numbers[i]).append("x").append(numbers[j]);
				macros.emplace_back(str1, str2);
				//macros.emplace_back(StrUtils::Concat("MF", numbers[i], "x", numbers[j]), StrUtils::Concat("min16float", numbers[i], "x", numbers[j]));
			}
		}
	}
	else {
		macros.emplace_back("MF", "float");

		for (uint32_t i = 0; i < 4; ++i)
		{
			CStdStringA str1, str2;
			str1 = "MF";
			str1.append(numbers[i]);
			str2 = "min16float";
			str2.append(numbers[i]);
			macros.emplace_back(str1, str2);
			//macros.emplace_back(StrUtils::Concat("MF", numbers[i]), StrUtils::Concat("float", numbers[i]));
			macros.emplace_back(std::string("MF").append(numbers[i]), std::string("float").append(numbers[i]));
			for (uint32_t j = 0; j < 4; ++j) {
				macros.emplace_back(std::string("MF").append(numbers[i]).append( "x").append( numbers[j]), std::string("float").append( numbers[i]).append("x").append( numbers[j]));
				//macros.emplace_back(StrUtils::Concat("MF", numbers[i], "x", numbers[j]), StrUtils::Concat("float", numbers[i], "x", numbers[j]));
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////
	//
	// 内联常量
	// 
	////////////////////////////////////////////////////////////////////////////////////////////////////////
	if (isInlineParams && inlineParams) {
		phmap::flat_hash_set<std::wstring> paramNames;
		for (const auto& d : desc.params) {
			const std::wstring& name = *paramNames.emplace(CCharsetConverter::UTF8ToUTF16(d.name)).first;

			auto it = inlineParams->find(name);
			if (it == inlineParams->end()) {
				if (d.constant.index() == 0) {
					macros.emplace_back(d.name, std::to_string(std::get<0>(d.constant).defaultValue));
				}
				else {
					macros.emplace_back(d.name, std::to_string(std::get<1>(d.constant).defaultValue));
				}
			}
			else {
				if (d.constant.index() == 0) {
					macros.emplace_back(d.name, std::to_string(it->second));
				}
				else {
					macros.emplace_back(d.name, std::to_string((int)std::lroundf(it->second)));
				}
			}
		}

		for (const auto& pair : *inlineParams) {
			if (!paramNames.contains(pair.first)) {
				return 1;
			}
		}

		result.push_back('\n');
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////
	//
	// 内置函数
	// 
	////////////////////////////////////////////////////////////////////////////////////////////////////////
	result.append(R"(uint __Bfe(uint src, uint off, uint bits) { uint mask = (1u << bits) - 1; return (src >> off) & mask; }
uint __BfiM(uint src, uint ins, uint bits) { uint mask = (1u << bits) - 1; return (ins & mask) | (src & (~mask)); }
uint2 Rmp8x8(uint a) { return uint2(__Bfe(a, 1u, 3u), __BfiM(__Bfe(a, 3u, 3u), a, 1u)); }
uint2 GetInputSize() { return __inputSize; }
float2 GetInputPt() { return __inputPt; }
uint2 GetOutputSize() { return __outputSize; }
float2 GetOutputPt() { return __outputPt; }
float2 GetScale() { return __scale; }
)");

	if (desc.flags & ShaderFlags::UseDynamic) {
		result.append(R"(uint GetFrameCount() { return __frameCount; }

)");
	}
	else {
		result.push_back('\n');
	}


	for (std::string_view commonBlock : commonBlocks) {
		result.append(commonBlock);
		result.push_back('\n');
	}

	result.append(passBlock);
	if (result.back() == '\n') {
		result.push_back('\n');
	}
	else {
		result.append("\n\n");
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////
	//
	// 着色器入口
	// 
	////////////////////////////////////////////////////////////////////////////////////////////////////////
	if (passDesc.isPSStyle) {
		if (passDesc.outputs.size() <= 1) {
			std::string outputSize;
			std::string outputPt;
			if (passIdx == desc.passes.size()) {
				// 最后一个通道
				outputSize = "__outputSize";
				outputPt = "__outputPt";
			}
			else {
				outputSize = fmt::format("__pass{}OutputSize", passIdx);
				outputPt = fmt::format("__pass{}OutputPt", passIdx);
			}

			result.append(fmt::format(R"([numthreads(64, 1, 1)]
void __M(uint3 tid : SV_GroupThreadID, uint3 gid : SV_GroupID) {{
	uint2 gxy = (gid.xy << 4u) + Rmp8x8(tid.x);
	if (gxy.x >= {1}.x || gxy.y >= {1}.y) {{
		return;
	}}
	float2 pos = (gxy + 0.5f) * {2};
	float2 step = 8 * {2};

	{3}[gxy] = Pass{0}(pos);

	gxy.x += 8u;
	pos.x += step.x;
	if (gxy.x < {1}.x && gxy.y < {1}.y) {{
		{3}[gxy] = Pass{0}(pos);
	}}
	
	gxy.y += 8u;
	pos.y += step.y;
	if (gxy.x < {1}.x && gxy.y < {1}.y) {{
		{3}[gxy] = Pass{0}(pos);
	}}
	
	gxy.x -= 8u;
	pos.x -= step.x;
	if (gxy.x < {1}.x && gxy.y < {1}.y) {{
		{3}[gxy] = Pass{0}(pos);
	}}
}}
)", passIdx, outputSize, outputPt, desc.textures[passDesc.outputs[0]].name));
		}
		else {
			// 多渲染目标
			result.append(fmt::format(R"([numthreads(64, 1, 1)]
void __M(uint3 tid : SV_GroupThreadID, uint3 gid : SV_GroupID) {{
	uint2 gxy = (gid.xy << 4u) + Rmp8x8(tid.x);
	if (gxy.x >= __pass{0}OutputSize.x || gxy.y >= __pass{0}OutputSize.y) {{
		return;
	}}
	float2 pos = (gxy + 0.5f) * __pass{0}OutputPt;
	float2 step = 8 * __pass{0}OutputPt;
)", passIdx));
			for (int i = 0; i < passDesc.outputs.size(); ++i) {
				auto& texDesc = desc.textures[passDesc.outputs[i]];
				result.append(fmt::format("\t{} c{};\n",
					FORMAT_DESCS[(uint32_t)texDesc.format].srvTexelType, i));
			}

			std::string callPass = fmt::format("\tPass{}(pos, ", passIdx);

			for (int i = 0; i < passDesc.outputs.size() - 1; ++i) {
				callPass.append(fmt::format("c{}, ", i));
			}
			callPass.append(fmt::format("c{});\n", passDesc.outputs.size() - 1));
			for (int i = 0; i < passDesc.outputs.size(); ++i) {
				callPass.append(fmt::format("\t\t\t{}[gxy] = c{};\n", desc.textures[passDesc.outputs[i]].name, i));
			}

			result.append(fmt::format(R"({0}
	gxy.x += 8u;
	pos.x += step.x;
	if (gxy.x < __pass{1}OutputSize.x && gxy.y < __pass{1}OutputSize.y) {{
		{0}
	}}
	
	gxy.y += 8u;
	pos.y += step.y;
	if (gxy.x < __pass{1}OutputSize.x && gxy.y < __pass{1}OutputSize.y) {{
		{0}
	}}
	
	gxy.x -= 8u;
	pos.x -= step.x;
	if (gxy.x < __pass{1}OutputSize.x && gxy.y < __pass{1}OutputSize.y) {{
		{0}
	}}
}}
)", callPass, passIdx));
		}
	}
	else {
		// 大部分情况下 BLOCK_SIZE 都是 2 的整数次幂，这时将乘法转换为位移
		std::string blockStartExpr;
		
		if (passDesc.blockSize.first == passDesc.blockSize.second && HAS_SINGLE_BIT(passDesc.blockSize.first)) {
			uint32_t nShift = std::lroundf(std::log2f((float)passDesc.blockSize.first));
			blockStartExpr = fmt::format("(gid.xy << {})", nShift);
		}
		else {
			blockStartExpr = fmt::format("gid.xy * uint2({}, {})", passDesc.blockSize.first, passDesc.blockSize.second);
		}

		result.append(fmt::format(R"([numthreads({}, {}, {})]
void __M(uint3 tid : SV_GroupThreadID, uint3 gid : SV_GroupID) {{
	Pass{}({}, tid);
}}
)", passDesc.numThreads[0], passDesc.numThreads[1], passDesc.numThreads[2], passIdx, blockStartExpr));
	}

	return 0;
}

static uint32_t CompilePasses(
	ShaderDesc& desc,
	uint32_t flags,
	const SmallVector<std::string_view>& commonBlocks,
	const std::vector<std::string_view>& passBlocks,
	const phmap::flat_hash_map<std::wstring, float>* inlineParams
) {
	////////////////////////////////////////////////////////////////////////////////////////////////////////
	//
	// 所有通道共用的常量缓冲区
	// 
	////////////////////////////////////////////////////////////////////////////////////////////////////////

	std::string cbHlsl = R"(cbuffer __CB1 : register(b0) {
	uint2 __inputSize;
	uint2 __outputSize;
	float2 __inputPt;
	float2 __outputPt;
	float2 __scale;
)";

	// PS 样式需要获知输出纹理的尺寸
	// 最后一个通道不需要
	for (uint32_t i = 0, end = (uint32_t)desc.passes.size() - 1; i < end; ++i) {
		if (desc.passes[i].isPSStyle) {
			cbHlsl.append(fmt::format("\tuint2 __pass{0}OutputSize;\n\tfloat2 __pass{0}OutputPt;\n", i + 1));
		}
	}

	if (!(desc.flags & ShaderFlags::InlineParams)) {
		for (const auto& d : desc.params) {
			cbHlsl.append("\t")
				.append(d.constant.index() == 0 ? "float " : "int ")
				.append(d.name)
				.append(";\n");
		}
	}

	cbHlsl.append("};\n");

	if (desc.flags & ShaderFlags::UseDynamic) {
		cbHlsl.append("cbuffer __CB2 : register(b1) { uint __frameCount; };\n\n");
	}

	std::wstring sourcesPathName = L"sources\\";//trUtils::Concat(CommonSharedConstants::SOURCES_DIR, CCharsetConverter::UTF8ToUTF16(desc.name));
	sourcesPathName.append(CCharsetConverter::UTF8ToUTF16(desc.name));
	std::wstring sourcesPath = sourcesPathName.substr(0, sourcesPathName.find_last_of(L'\\'));
	
	if ((flags & ShaderCompilerFlags::SaveSources) && !XFILE::CDirectory::Exists(WToA(sourcesPath))) {
		if (!XFILE::CDirectory::Create(WToA(sourcesPath)))
			CLog::Log(LOGERROR, "Failed creating directory {}", WToA(sourcesPath));

		//HRESULT hr = wil::CreateDirectoryDeepNoThrow(sourcesPath.c_str());
		//if (FAILED(hr)) {
			//Logger::Get().ComError("创建 sources 文件夹失败", hr);
		
	}

	size_t delimPos = desc.name.find_last_of('\\');
	PassInclude passInclude(delimPos == std::string::npos
		? L"Shaders\\"
		: L"Shaders\\" + CCharsetConverter::UTF8ToUTF16(std::string_view(desc.name.c_str(), delimPos + 1)));

	// 并行生成代码和编译
	RunParallel([&](uint32_t id) {
		std::string source;
		std::vector<std::pair<std::string, std::string>> macros;
		if (id == 8)
			CLog::Log(LOGINFO, "pass8");
		if (GeneratePassSource(desc, id + 1, cbHlsl, commonBlocks, passBlocks[id], inlineParams, source, macros)) {
			//Logger::Get().Error(fmt::format("生成 Pass{} 失败", id + 1));
			return;
		}

		if (flags & ShaderCompilerFlags::SaveSources)
		{
			std::wstring fileName;
			if (desc.passes.size() == 1)
			{
				fileName = sourcesPathName.append(L".hlsl");

			}
			else
			{
				fmt::format(L"{}_Pass{}.hlsl", sourcesPathName, id + 1);
			}
			XFILE::CFile fle;
			//!WriteFile(fileName.c_str(), source.data(), source.size()))
			if (fle.OpenForWrite(WToA(fileName)) )
			{
				fle.Write(source.data(), source.size());
				//Logger::Get().Error(fmt::format("保存 Pass{} 源码失败", id + 1));
			}
		}

		if (FAILED(CompileComputeShader(source, "__M", &desc.passes[id].cso,
			fmt::format("{}_Pass{}.hlsl", desc.name, id + 1).c_str(), &passInclude, macros, flags & ShaderCompilerFlags::WarningsAreErrors))) {
			CLog::Log(LOGERROR, "Damn compile failed");
		}
		}, (uint32_t)passBlocks.size());

	for (const ShaderPassDesc& d : desc.passes) {
		if (!d.cso) {
			return 1;
		}
	}

	return 0;
}

static std::string ReadShaderSource(const std::wstring& ShaderName) noexcept {
	//std::wstring fileName = StrUtils::Concat(CommonSharedConstants::ShaderS_DIR, ShaderName, L".hlsl");
	assert(0);

	/*std::string source;
	if (!Win32Utils::ReadTextFile(fileName.c_str(), source)) {
		
		return {};
	}
	return source;*/
}

uint32_t CShaderFileLoader::Compile(ShaderDesc& desc, uint32_t flags, const phmap::flat_hash_map<std::wstring, float>* inlineParams)
{
	if (m_pFile == L"")
		return false;
	XFILE::CFile currentfile;
	currentfile.Exists(WToA(m_pFile));
	std::wstring currentpath = m_pFile;// GetFilePathExists(m_pFile.c_str());
	
	std::wifstream infile(currentpath.c_str());
	std::string source;
	bool cacheinit = Hasher::GetInstance().Initialize();
	XFILE::CFile file;
	std::string pFileNameA = WToA(currentpath);
	if (!file.Exists(pFileNameA))
		return 0;
	file.Open(pFileNameA);
	int64_t filelength = file.GetLength();
	char* pData1 = (char*)malloc(filelength);
	if (file.Read(pData1, filelength) == filelength)
	{
		source = pData1;
		free(pData1);
	}
	else//if (!ReadTextFile(currentpath.c_str(), source))
	{
		CLog::Log(LOGERROR, "couldn't read for compile {}", WToA(currentpath).c_str());
		return false;
	}
	
	bool noCompile = flags & ShaderCompilerFlags::NoCompile;
	bool noCache = noCompile || (flags & ShaderCompilerFlags::NoCache);

	RemoveComments(source);

	std::wstring hash;
	if (!noCache)
	{
		hash = GetHash(source, desc.flags & ShaderFlags::InlineParams ? inlineParams : nullptr);
			if (!hash.empty()) {
				
				if (ShaderCache::GetInstance().Load(AToW(desc.name), hash, desc)) {
					// 已从缓存中读取
					return 0;
				}
			}
	}

	std::string_view sourceView(source);

	if (!CheckMagic(sourceView))
	{
		return false;
	}

	enum class BlockType
	{
		Header,
		Parameter,
		Texture,
		Sampler,
		Common,
		Pass
	};

	std::string_view headerBlock;
	SmallVector<std::string_view > paramBlocks;
	SmallVector<std::string_view > textureBlocks;
	SmallVector<std::string_view > samplerBlocks;
	SmallVector<std::string_view > commonBlocks;
	std::vector<std::string_view > passBlocks;

	BlockType curBlockType = BlockType::Header;
	size_t curBlockOff = 0;

	auto completeCurrentBlock = [&](size_t len, BlockType newBlockType)
	{
	  std::string currentBlock;
		std::string_view currentBlockView;
		char cur;
		switch (curBlockType)
		{
		case BlockType::Header:
			headerBlock = sourceView.substr(curBlockOff, len);
			break;
		case BlockType::Parameter:
			paramBlocks.push_back(sourceView.substr(curBlockOff, len));
			break;
		case BlockType::Texture:
			textureBlocks.push_back(sourceView.substr(curBlockOff, len));
			break;
		case BlockType::Sampler:
			samplerBlocks.push_back(sourceView.substr(curBlockOff, len));
			break;
		case BlockType::Common:
			commonBlocks.push_back(sourceView.substr(curBlockOff, len));
			break;
		case BlockType::Pass:
			currentBlock = sourceView.substr(curBlockOff, len);
			while (true)
			{
				char cur = currentBlock[currentBlock.length() - 1];
				if (cur != '}' && (cur != '\n'))
				{
					currentBlock.resize(currentBlock.length() - 1);
				}
				else 
					break;
			}
			passBlocks.push_back(sourceView.substr(curBlockOff, currentBlock.size()));
			break;
		default:
			assert(false);
			break;
		}

		curBlockType = newBlockType;
		curBlockOff += len;
	};

	bool newLine = true;
	std::string_view t = sourceView;
	while (t.size() > 5)
	{
		if (newLine)
		{
			// contains newlines
			size_t len = t.data() - sourceView.data() - curBlockOff + 1;

			if (CheckNextToken<true>(t, META_INDICATOR))
			{
				std::string_view token;
				if (GetNextToken<false>(t, token))
				{
					return 1;
				}

				std::string blockType = ToUpper(token);

				if (blockType == "PARAMETER")
				{
					completeCurrentBlock(len, BlockType::Parameter);
				}
				else if (blockType == "TEXTURE")
				{
					completeCurrentBlock(len, BlockType::Texture);
				}
				else if (blockType == "SAMPLER")
				{
					completeCurrentBlock(len, BlockType::Sampler);
				}
				else if (blockType == "COMMON")
				{
					completeCurrentBlock(len, BlockType::Common);
				}
				else if (blockType == "PASS")
				{
					completeCurrentBlock(len, BlockType::Pass);
				}
			}

			if (t.size() <= 5)
			{
				break;
			}
		}
		else
		{
			t.remove_prefix(1);
		}

		newLine = t[0] == '\n';
	}

	completeCurrentBlock(sourceView.size() - curBlockOff, BlockType::Header);

	// PASS
	if (!noCompile && passBlocks.empty())
	{
		return 1;
	}

	if (ResolveHeader(headerBlock, desc, noCompile))
	{
		CLog::Log(LOGERROR, "{} Missing header", __FUNCTION__);
		return 1;
	}

	desc.params.clear();

	for (size_t i = 0; i < paramBlocks.size(); ++i) {
		if (ResolveParameter(paramBlocks[i], desc)) {
			CLog::Log(LOGERROR, "{} Missing Parameter {}", __FUNCTION__, i + 1);
			return 1;
		}
	}

	desc.textures.clear();
	// texture INPUT
	{
		auto& inputDesc = desc.textures.emplace_back();
		inputDesc.name = "INPUT";
		inputDesc.format = ShaderIntermediateTextureFormat::R8G8B8A8_UNORM;
		inputDesc.sizeExpr.first = "INPUT_WIDTH";
		inputDesc.sizeExpr.second = "INPUT_HEIGHT";
	}
	// texture OUTPUT
	{
		auto& outputDesc = desc.textures.emplace_back();
		outputDesc.name = "OUTPUT";
		outputDesc.format = ShaderIntermediateTextureFormat::R8G8B8A8_UNORM;
	}

	for (size_t i = 0; i < textureBlocks.size(); ++i) {
		if (ResolveTexture(textureBlocks[i], desc)) {
			CLog::Log(LOGERROR, "{} Missing Texture {}", __FUNCTION__, i + 1);
			return 1;
		}
	}

	if (!noCompile) {
		desc.samplers.clear();
		for (size_t i = 0; i < samplerBlocks.size(); ++i) {
			if (ResolveSampler(samplerBlocks[i], desc)) {
				CLog::Log(LOGERROR, "{} Sampler Texture {}", __FUNCTION__, i + 1);
				return 1;
			}
		}
	}
	
	{
		// Make sure there are no duplicate names
		phmap::flat_hash_set<std::string> names;
		for (const auto& d : desc.params) {
			if (names.find(d.name) != names.end()) {
				CLog::Log(LOGERROR, "{} parmas duplicate detected {}", __FUNCTION__);
				return 1;
			}
			names.insert(d.name);
		}
		for (const auto& d : desc.textures) {
			if (names.find(d.name) != names.end()) {
				CLog::Log(LOGERROR, "{} textures duplicate detected {}", __FUNCTION__);
				return 1;
			}
			names.insert(d.name);
		}
		for (const auto& d : desc.samplers) {
			if (names.find(d.name) != names.end()) {
				CLog::Log(LOGERROR, "{} samplers duplicate detected {}", __FUNCTION__);
				return 1;
			}
			names.insert(d.name);
		}
	}

	if (!noCompile) {
		for (size_t i = 0; i < commonBlocks.size(); ++i) {
			if (ResolveCommon(commonBlocks[i])) {
				CLog::Log(LOGERROR, "{} Parsing Common#{} block failed", __FUNCTION__, i + 1);
				return 1;
			}
		}

		desc.passes.clear();
		if (ResolvePasses(passBlocks, desc)) {
			CLog::Log(LOGERROR, "{} Failed to parse Pass block", __FUNCTION__);
			return 1;
		}

		if (CompilePasses(desc, flags, commonBlocks, passBlocks, inlineParams)) {
			CLog::Log(LOGERROR, "{} Compile passes failed", __FUNCTION__);
			return 1;
		}

		if (!noCache && !hash.empty()) {
			ShaderCache::GetInstance().Save(AToW(desc.name), hash, desc);
		}
	}
	return 0;
}

HRESULT CShaderFileLoader::Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes)
{
	XFILE::CFile file;
	if (!file.Exists(pFileName))
		return E_FAIL;
	file.Open(pFileName);
	int64_t filelength = file.GetLength();
  char *pData1 = (char*)malloc(filelength);
	if (file.Read(pData1, filelength) != filelength)
		return E_FAIL;


	//std::wstring filepath = Utility::GetFilePathExists(Utility::UTF8ToWideString(pFileName).c_str());
	//if (filepath == L"")
	//	return E_FAIL;
	//bool res;
	std::string includeFile = pData1;
	//res = Utility::ReadTextFile(filepath.c_str(), includeFile);
	free(pData1);

	RemoveComments(includeFile);
	int64_t length = includeFile.size();
	void* pData = malloc(length);
	memcpy(pData, includeFile.data(), includeFile.size());

	*ppData = pData;
	*pBytes = length;

	return S_OK;
}

HRESULT CShaderFileLoader::Close(LPCVOID pData)
{
	free((void*)pData);
	return S_OK;
}