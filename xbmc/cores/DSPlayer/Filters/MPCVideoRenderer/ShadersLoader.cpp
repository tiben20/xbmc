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
//#include "utility.h"
#include "filesystem/File.h"
#include "corecrt_io.h"
//#include "FileUtility.h"
#include <bitset>
#include <unordered_set>
#include <charconv>
#include "ctype.h"
#include "helper.h"
#include "shaders.h"

static constexpr
const char* META_INDICATOR = "//!";

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

std::string Bin2Hex(BYTE* data, size_t len)
{
	if (!data || len == 0) {
		return {};
	}

	static char oct2Hex[16] = {
		'0','1','2','3','4','5','6','7',
		'8','9','a','b','c','d','e','f'
	};

	std::string result(len * 2, 0);
	char* pResult = &result[0];

	for (size_t i = 0; i < len; ++i) {
		BYTE b = *data++;
		*pResult++ = oct2Hex[(b >> 4) & 0xf];
		*pResult++ = oct2Hex[b & 0xf];
	}

	return result;
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

template < bool IncludeNewLine>
static void RemoveLeadingBlanks(std::string_view& source)
{
	size_t i = 0;
	for (; i < source.size(); ++i)
	{
		if constexpr (IncludeNewLine)
		{
			if (!std::isspace(static_cast<unsigned char> (source[i])))
			{
				break;
			}
		}
		else
		{
			char c = source[i];
			if (c != ' ' && c != '\t')
			{
				break;
			}
		}
	}

	source.remove_prefix(i);
}

template < bool AllowNewLine>
static bool CheckNextToken(std::string_view& source, std::string_view token)
{
	RemoveLeadingBlanks<AllowNewLine>(source);

	if (!source._Starts_with(token))
	{
		return false;
	}

	source.remove_prefix(token.size());
	return true;
}

template < bool AllowNewLine>
static UINT GetNextToken(std::string_view& source, std::string_view& value)
{
	RemoveLeadingBlanks<AllowNewLine>(source);

	if (source.empty())
	{
		return 2;
	}

	char cur = source[0];

	if (std::isalpha(static_cast<unsigned char> (cur)) || cur == '_')
	{
		size_t j = 1;
		for (; j < source.size(); ++j)
		{
			cur = source[j];

			if (!std::isalnum(static_cast<unsigned char> (cur)) && cur != '_')
			{
				break;
			}
		}

		value = source.substr(0, j);
		source.remove_prefix(j);
		return 0;
	}

	if constexpr (AllowNewLine)
	{
		return 1;
	}
	else
	{
		return cur == '\n' ? 2 : 1;
	}
}

UINT GetNextString(std::string_view& source, std::string_view& value)
{
	RemoveLeadingBlanks<false>(source);
	size_t pos = source.find('\n');

	value = source.substr(0, pos);

	Trim(value);
	if (value.empty())
	{
		return 1;
	}

	source.remove_prefix(std::min(pos + 1, source.size()));
	return 0;
}

template < typename T>
static UINT GetNextNumber(std::string_view& source, T& value)
{
	RemoveLeadingBlanks<false>(source);

	if (source.empty())
	{
		return 1;
	}

	const auto& result = std::from_chars(source.data(), source.data() + source.size(), value);
	if ((int)result.ec)
	{
		return 1;
	}

	// Parsing succeeded
	source.remove_prefix(result.ptr - source.data());
	return 0;
}

UINT GetNextExpr(std::string_view& source, std::string& expr)
{
	RemoveLeadingBlanks<false>(source);
	size_t size = std::min(source.find('\n') + 1, source.size());

	// remove whitespace
	expr.resize(size);

	size_t j = 0;
	for (size_t i = 0; i < size; ++i)
	{
		char c = source[i];
		if (!isspace(c))
		{
			expr[j++] = c;
		}
	}

	expr.resize(j);

	if (expr.empty())
	{
		return 1;
	}

	source.remove_prefix(size);
	return 0;
}

UINT RemoveComments(std::string& source)
{
	//Make sure to end with a newline
	if (source.back() != '\n')
	{
		source.push_back('\n');
	}

	std::string result;
	result.reserve(source.size());

	int j = 0;
	// Process the last two characters individually
	for (size_t i = 0, end = source.size() - 2; i < end; ++i)
	{
		if (source[i] == '/')
		{
			if (source[i + 1] == '/' && source[i + 2] != '!')
			{
				// line comment
				i += 2;

				// No need to handle out - of - bounds because it must end with a newline
				while (source[i] != '\n')
				{
					++i;
				}

				//Line breaks are preserved
				source[j++] = '\n';

				continue;
			}
			else if (source[i + 1] == '*')
			{
				// block comment
				i += 2;

				while (true)
				{
					if (++i >= source.size())
					{
						// not closed
						return 1;
					}

					if (source[i - 1] == '*' && source[i] == '/')
					{
						break;
					}
				}

				// end of file
				if (i >= source.size() - 2)
				{
					source.resize(j);
					return 0;
				}

				continue;
			}
		}

		source[j++] = source[i];
	}

	source[j++] = source[source.size() - 2];
	source.resize(j);
	return 0;
}

bool CheckMagic(std::string_view& source)
{
	std::string_view token;
	if (!CheckNextToken<true>(source, META_INDICATOR))
		return false;

	if (!CheckNextToken<false>(source, "MPC"))
		return false;

	if (!CheckNextToken<false>(source, "SCALER"))
		return false;

	if (GetNextToken<false>(source, token) != 2)
		return false;

	if (source.empty())
	{
		return false;
	}

	return true;
}

UINT ResolveHeader(std::string_view block, ShaderDesc& desc)
{
	// Required options?VERSION
	// optional options?OUTPUT_WIDTH?OUTPUT_HEIGHT

	std::bitset<5> processed;

	std::string_view token;

	while (true)
	{
		if (!CheckNextToken<true>(block, META_INDICATOR))
		{
			break;
		}

		if (GetNextToken<false>(block, token))
		{
			return 1;
		}

		std::string t = ToUpper(token);

		if (t == "VERSION")
		{
			if (processed[0])
			{
				return 1;
			}

			processed[0] = true;

			UINT version;
			if (GetNextNumber(block, version))
			{
				return 1;
			}

			if (version != SCALERCOMPILE_VERSION)
			{
				return 1;
			}

			if (GetNextToken<false>(block, token) != 2)
			{
				return 1;
			}
		}
		else if (t == "OUTPUT_WIDTH")
		{
			if (processed[1])
			{
				return 1;
			}

			processed[1] = true;

			if (GetNextExpr(block, desc.outSizeExpr.first))
			{
				return 1;
			}
		}
		else if (t == "OUTPUT_HEIGHT")
		{
			if (processed[2])
			{
				return 1;
			}

			processed[2] = true;

			if (GetNextExpr(block, desc.outSizeExpr.second))
			{
				return 1;
			}
		}
		else if (t == "SCALER_TYPE")
		{
			if (processed[3])
			{
				return 1;
			}

			processed[3] = true;

			if (GetNextExpr(block, desc.outSizeExpr.second))
			{
				return 1;
			}
		}
		else if (t == "DESCRIPTION")
		{
			if (processed[4])
			{
				return 1;
			}

			processed[4] = true;
			std::string_view descrip;
			if (GetNextString(block, descrip))
			{
				desc.shaderDescription = descrip;
				return 1;
			}

			desc.shaderDescription = descrip;
		}
		else
		{
			return 1;
		}
	}

	// HEADER block without code section
	if (GetNextToken<true>(block, token) != 2)
	{
		return 1;
	}

	if (!processed[0] || (processed[1] ^ processed[2]))
	{
		return 1;
	}

	return 0;
}

UINT ResolveConstant(std::string_view block, ShaderDesc& desc)
{
	// optional options?VALUE?DEFAULT?LABEL?MIN?MAX, DYNAMIC
	// VALUE Mutually exclusive with other options
	// DEFAULT is required if no VALUE

	std::bitset<6> processed;

	std::string_view token;

	if (!CheckNextToken<true>(block, META_INDICATOR))
	{
		return 1;
	}

	if (!CheckNextToken<false>(block, "CONSTANT"))
	{
		return 1;
	}

	if (GetNextToken<false>(block, token) != 2)
	{
		return 1;
	}

	ShaderConstantDesc desc1{};
	ShaderValueConstantDesc desc2{};

	std::string_view defaultValue;
	std::string_view minValue;
	std::string_view maxValue;

	while (true)
	{
		if (!CheckNextToken<true>(block, META_INDICATOR))
		{
			break;
		}

		if (GetNextToken<false>(block, token))
		{
			return 1;
		}

		std::string t = ToUpper(token);

		if (t == "VALUE")
		{
			for (int i = 0; i < 5; ++i)
			{
				if (processed[i])
				{
					return 1;
				}
			}

			processed[0] = true;

			if (GetNextExpr(block, desc2.valueExpr))
				return 1;

		}
		else if (t == "DEFAULT")
		{
			if (processed[0] || processed[1] || processed[5])
				return 1;

			processed[1] = true;

			if (GetNextString(block, defaultValue))
				return 1;

		}
		else if (t == "LABEL")
		{
			if (processed[0] || processed[2])
			{
				return 1;
			}

			processed[2] = true;

			std::string_view t;
			if (GetNextString(block, t))
			{
				return 1;
			}

			desc1.label = t;
		}
		else if (t == "MIN")
		{
			if (processed[0] || processed[3] || processed[5])
			{
				return 1;
			}

			processed[3] = true;

			if (GetNextString(block, minValue))
			{
				return 1;
			}
		}
		else if (t == "MAX")
		{
			if (processed[0] || processed[4] || processed[5])
			{
				return 1;
			}

			processed[4] = true;

			if (GetNextString(block, maxValue))
			{
				return 1;
			}
		}
		else if (t == "DYNAMIC")
		{
			for (int i = 1; i < 6; ++i)
			{
				if (processed[i])
				{
					return 1;
				}
			}

			processed[5] = true;

			if (GetNextToken<false>(block, token) != 2)
			{
				return 1;
			}
		}
		else
		{
			return 1;
		}
	}

	// DYNAMIC must appear with VALUE
	if (!processed[0] && processed[5])
	{
		return 1;
	}

	// VALUE or DEFAULT must exist
	if (processed[0] == processed[1])
	{
		return 1;
	}

	// code section
	if (GetNextToken<true>(block, token))
	{
		return 1;
	}

	if (token == "float")
	{
		if (processed[0])
		{
			desc2.type = ShaderConstantType::Float;
		}
		else
		{
			desc1.type = ShaderConstantType::Float;

			if (!defaultValue.empty())
			{
				desc1.defaultValue = 0.0f;
				if (GetNextNumber(defaultValue, std::get<float>(desc1.defaultValue)))
				{
					return 1;
				}
			}

			if (!minValue.empty())
			{
				float value;
				if (GetNextNumber(minValue, value))
				{
					return 1;
				}

				if (!defaultValue.empty() && std::get<float>(desc1.defaultValue) < value)
				{
					return 1;
				}

				desc1.minValue = value;
			}

			if (!maxValue.empty())
			{
				float value;
				if (GetNextNumber(maxValue, value))
				{
					return 1;
				}

				if (!defaultValue.empty() && std::get<float>(desc1.defaultValue) > value)
				{
					return 1;
				}

				if (!minValue.empty() && std::get<float>(desc1.minValue) > value)
				{
					return 1;
				}

				desc1.maxValue = value;
			}
		}
	}
	else if (token == "int")
	{
		if (processed[0])
		{
			desc2.type = ShaderConstantType::Int;
		}
		else
		{
			desc1.type = ShaderConstantType::Int;

			if (!defaultValue.empty())
			{
				desc1.defaultValue = 0;
				if (GetNextNumber(defaultValue, std::get<int>(desc1.defaultValue)))
				{
					return 1;
				}
			}

			if (!minValue.empty())
			{
				int value;
				if (GetNextNumber(minValue, value))
				{
					return 1;
				}

				if (!defaultValue.empty() && std::get<int>(desc1.defaultValue) < value)
				{
					return 1;
				}

				desc1.minValue = value;
			}

			if (!maxValue.empty())
			{
				int value;
				if (GetNextNumber(maxValue, value))
				{
					return 1;
				}

				if (!defaultValue.empty() && std::get<int>(desc1.defaultValue) > value)
				{
					return 1;
				}

				if (!minValue.empty() && std::get<int>(desc1.minValue) > value)
				{
					return 1;
				}

				desc1.maxValue = value;
			}
		}
	}
	else
	{
		return 1;
	}

	if (GetNextToken<true>(block, token))
	{
		return 1;
	}

	(processed[0] ? desc2.name : desc1.name) = token;

	if (!CheckNextToken<true>(block, ";"))
	{
		return 1;
	}

	if (GetNextToken<true>(block, token) != 2)
	{
		return 1;
	}

	if (processed[0])
	{
		if (processed[5])
		{
			desc.dynamicValueConstants.emplace_back(std::move(desc2));
		}
		else
		{
			desc.valueConstants.emplace_back(std::move(desc2));
		}
	}
	else
	{
		desc.constants.emplace_back(std::move(desc1));
	}

	return 0;
}

UINT ResolveTexture(std::string_view block, ShaderDesc& desc)
{
	// If the name is INPUT can not have any options, including SOURCE can not have any other options
	// otherwise required options: FORMAT
	// optional options: WIDTH, HEIGHT

	ShaderIntermediateTextureDesc& texDesc = desc.textures.emplace_back();

	std::bitset<4> processed;

	std::string_view token;

	if (!CheckNextToken<true>(block, META_INDICATOR))
	{
		return 1;
	}

	if (!CheckNextToken<false>(block, "TEXTURE"))
	{
		return 1;
	}

	if (GetNextToken<false>(block, token) != 2)
	{
		return 1;
	}

	while (true)
	{
		if (!CheckNextToken<true>(block, META_INDICATOR))
		{
			break;
		}

		if (GetNextToken<false>(block, token))
		{
			return 1;
		}

		std::string t = ToUpper(token);

		if (t == "SOURCE")
		{
			if (processed.any())
			{
				return 1;
			}

			processed[0] = true;

			if (GetNextString(block, token))
			{
				return 1;
			}

			texDesc.source = token;
		}
		else if (t == "FORMAT")
		{
			if (processed[0] || processed[1])
			{
				return 1;
			}

			processed[1] = true;

			if (GetNextString(block, token))
			{
				return 1;
			}

			static std::unordered_map<std::string, ShaderIntermediateTextureFormat> formatMap = {
		{
					"R8_UNORM", ShaderIntermediateTextureFormat::R8_UNORM
				},
				{
					"R16_UNORM", ShaderIntermediateTextureFormat::R16_UNORM
				},
				{
					"R16_FLOAT", ShaderIntermediateTextureFormat::R16_FLOAT
				},
				{
					"R8G8_UNORM", ShaderIntermediateTextureFormat::R8G8_UNORM
				},
				{
					"B5G6R5_UNORM", ShaderIntermediateTextureFormat::B5G6R5_UNORM
				},
				{
					"R16G16_UNORM", ShaderIntermediateTextureFormat::R16G16_UNORM
				},
				{
					"R16G16_FLOAT", ShaderIntermediateTextureFormat::R16G16_FLOAT
				},
				{
					"R8G8B8A8_UNORM", ShaderIntermediateTextureFormat::R8G8B8A8_UNORM
				},
				{
					"B8G8R8A8_UNORM", ShaderIntermediateTextureFormat::B8G8R8A8_UNORM
				},
				{
					"R10G10B10A2_UNORM", ShaderIntermediateTextureFormat::R10G10B10A2_UNORM
				},
				{
					"R32_FLOAT", ShaderIntermediateTextureFormat::R32_FLOAT
				},
				{
					"R11G11B10_FLOAT", ShaderIntermediateTextureFormat::R11G11B10_FLOAT
				},
				{
					"R32G32_FLOAT", ShaderIntermediateTextureFormat::R32G32_FLOAT
				},
				{
					"R16G16B16A16_UNORM", ShaderIntermediateTextureFormat::R16G16B16A16_UNORM
				},
				{
					"R16G16B16A16_FLOAT", ShaderIntermediateTextureFormat::R16G16B16A16_FLOAT
				},
				{
					"R32G32B32A32_FLOAT", ShaderIntermediateTextureFormat::R32G32B32A32_FLOAT
				}
			};

			auto it = formatMap.find(std::string(token));
			if (it == formatMap.end())
			{
				return 1;
			}

			texDesc.format = it->second;
		}
		else if (t == "WIDTH")
		{
			if (processed[0] || processed[2])
			{
				return 1;
			}

			processed[2] = true;

			if (GetNextExpr(block, texDesc.sizeExpr.first))
			{
				return 1;
			}
		}
		else if (t == "HEIGHT")
		{
			if (processed[0] || processed[3])
			{
				return 1;
			}

			processed[3] = true;

			if (GetNextExpr(block, texDesc.sizeExpr.second))
			{
				return 1;
			}
		}
		else
		{
			return 1;
		}
	}

	// WIDTH and HEIGHT must be paired
	if (processed[2] ^ processed[3])
	{
		return 1;
	}

	// code section
	if (!CheckNextToken<true>(block, "Texture2D"))
	{
		return 1;
	}

	if (GetNextToken<true>(block, token))
	{
		return 1;
	}

	if (token == "INPUT")
	{
		if (processed[1] || processed[2])
		{
			return 1;
		}

		// INPUT is already the first element
		desc.textures.pop_back();
	}
	else
	{
		// otherwise FORMAT and SOURCE must be chosen
		if (processed[0] == processed[1])
		{
			return 1;
		}

		texDesc.name = token;
	}

	if (!CheckNextToken<true>(block, ";"))
	{
		return 1;
	}

	if (GetNextToken<true>(block, token) != 2)
	{
		return 1;
	}

	return 0;
}

UINT ResolveSampler(std::string_view block, ShaderDesc& desc)
{
	// Required: FILTER
	// optional: ADDRESS

	ShaderSamplerDesc& samDesc = desc.samplers.emplace_back();

	std::bitset<2> processed;

	std::string_view token;

	if (!CheckNextToken<true>(block, META_INDICATOR))
	{
		return 1;
	}

	if (!CheckNextToken<false>(block, "SAMPLER"))
	{
		return 1;
	}

	if (GetNextToken<false>(block, token) != 2)
	{
		return 1;
	}

	while (true)
	{
		if (!CheckNextToken<true>(block, META_INDICATOR))
		{
			break;
		}

		if (GetNextToken<false>(block, token))
		{
			return 1;
		}

		std::string t = ToUpper(token);

		if (t == "FILTER")
		{
			if (processed[0])
			{
				return 1;
			}

			processed[0] = true;

			if (GetNextString(block, token))
			{
				return 1;
			}

			std::string filter = ToUpper(token);

			if (filter == "LINEAR")
			{
				samDesc.filterType = ShaderSamplerFilterType::Linear;
			}
			else if (filter == "POINT")
			{
				samDesc.filterType = ShaderSamplerFilterType::Point;
			}
			else
			{
				return 1;
			}
		}
		else if (t == "ADDRESS")
		{
			if (processed[1])
			{
				return 1;
			}

			processed[1] = true;

			if (GetNextString(block, token))
			{
				return 1;
			}

			std::string filter = ToUpper(token);

			if (filter == "CLAMP")
			{
				samDesc.addressType = ShaderSamplerAddressType::Clamp;
			}
			else if (filter == "WRAP")
			{
				samDesc.addressType = ShaderSamplerAddressType::Wrap;
			}
			else
			{
				return 1;
			}
		}
		else
		{
			return 1;
		}
	}

	if (!processed[0])
	{
		return 1;
	}

	// code section
	if (!CheckNextToken<true>(block, "SamplerState"))
	{
		return 1;
	}

	if (GetNextToken<true>(block, token))
	{
		return 1;
	}

	samDesc.name = token;

	if (!CheckNextToken<true>(block, ";"))
	{
		return 1;
	}

	if (GetNextToken<true>(block, token) != 2)
	{
		return 1;
	}

	return 0;
}

UINT ResolveCommon(std::string_view& block)
{
	// no options

	if (!CheckNextToken<true>(block, META_INDICATOR))
	{
		return 1;
	}

	if (!CheckNextToken<false>(block, "COMMON"))
	{
		return 1;
	}

	if (CheckNextToken<true>(block, META_INDICATOR))
	{
		return 1;
	}

	if (block.empty())
	{
		return 1;
	}

	return 0;
}

UINT ResolvePass(std::string_view block, ShaderDesc& desc, std::vector<std::string >& passSources, const std::string& commonHlsl)
{
	std::string_view token;

	if (!CheckNextToken<true>(block, META_INDICATOR))
	{
		return 1;
	}

	if (!CheckNextToken<false>(block, "PASS"))
	{
		return 1;
	}

	size_t index;
	if (GetNextNumber(block, index))
	{
		return 1;
	}

	if (GetNextToken<false>(block, token) != 2)
	{
		return 1;
	}

	if (index == 0 || index >= block.size() + 1)
	{
		return 1;
	}

	if (index > passSources.size() || !passSources[index - 1].empty())
	{
		return 1;
	}

	ShaderPassDesc& passDesc = desc.passes[index - 1];

	// used to check for duplicate textures in input and output
	std::unordered_map<std::string_view, UINT> texNames;
	for (size_t i = 0; i < desc.textures.size(); ++i)
	{
		texNames.emplace(desc.textures[i].name, (UINT)i);
	}

	std::bitset<2> processed;

	while (true)
	{
		if (!CheckNextToken<true>(block, META_INDICATOR))
		{
			break;
		}

		if (GetNextToken<false>(block, token))
		{
			return 1;
		}

		std::string t = ToUpper(token);

		if (t == "BIND")
		{
			if (processed[0])
			{
				return 1;
			}

			processed[0] = true;

			std::string binds;
			if (GetNextExpr(block, binds))
			{
				return 1;
			}

			std::vector<std::string_view > inputs = Split(binds, ',');
			for (const std::string_view& input : inputs)
			{
				auto it = texNames.find(input);
				if (it == texNames.end())
				{
					// Texture name not found
					return 1;
				}

				passDesc.inputs.push_back(it->second);
				texNames.erase(it);
			}
		}
		else if (t == "SAVE")
		{
			if (processed[1])
			{
				return 1;
			}

			processed[1] = true;

			std::string saves;
			if (GetNextExpr(block, saves))
			{
				return 1;
			}

			std::vector<std::string_view > outputs = Split(saves, ',');
			if (outputs.size() > 8)
			{
				// up to 8 outputs
				return 1;
			}

			for (const std::string_view& output : outputs)
			{
				// INPUT cannot be used as output
				if (output == "INPUT")
				{
					return 1;
				}

				auto it = texNames.find(output);
				if (it == texNames.end())
				{
					// Texture name not found
					return 1;
				}

				passDesc.outputs.push_back(it->second);
				texNames.erase(it);
			}
		}
		else
		{
			return 1;
		}
	}

	std::string& passHlsl = passSources[index - 1];
	passHlsl.reserve(size_t((commonHlsl.size() + block.size() + passDesc.inputs.size() * 30) * 1.5));

	for (int i = 0; i < passDesc.inputs.size(); ++i)
	{
		passHlsl.append(fmt::format("Texture2D {}:register(t{});", desc.textures[passDesc.inputs[i]].name, i));
	}

	passHlsl.append(commonHlsl).append(block);

	if (passHlsl.back() != '\n')
	{
		passHlsl.push_back('\n');
	}

	// main function
	if (passDesc.outputs.size() <= 1)
	{
		passHlsl.append(fmt::format("float4 __M(float4 p:SV_POSITION,float2 c:TEXCOORD):SV_TARGET"
			"{{return Pass{}(c);}}", index));
	}
	else
	{
		// multiple render targets
		passHlsl.append("void __M(float4 p:SV_POSITION,float2 c:TEXCOORD,out float4 t0:SV_TARGET0,out float4 t1:SV_TARGET1");
		for (int i = 2; i < passDesc.outputs.size(); ++i)
		{
			passHlsl.append(fmt::format(",out float4 t{0}:SV_TARGET{0}", i));
		}

		passHlsl.append(fmt::format("){{Pass{}(c,t0,t1", index));
		for (int i = 2; i < passDesc.outputs.size(); ++i)
		{
			passHlsl.append(fmt::format(",t{}", i));
		}

		passHlsl.append(");}");
	}

	return 0;
}

UINT ResolvePasses(const std::vector<std::string_view >& blocks, const std::vector<std::string_view >& commons, ShaderDesc& desc, ID3DInclude* d3dinc)
{
	// optional: BIND, SAVE

	std::string commonHlsl;

	// estimate the space needed
	size_t reservedSize = (desc.constants.size() + desc.samplers.size()) * 30;
	for (const auto& c : commons)
	{
		reservedSize += c.size();
	}

	commonHlsl.reserve(size_t(reservedSize * 1.5f));

	if (!desc.constants.empty() || !desc.valueConstants.empty())
	{
		// constant buffer
		commonHlsl.append("cbuffer __C:register(b0){");
		for (const auto& d : desc.constants)
		{
			commonHlsl.append(d.type == ShaderConstantType::Int ? "int " : "float ")
				.append(d.name)
				.append(";");
		}

		for (const auto& d : desc.valueConstants)
		{
			commonHlsl.append(d.type == ShaderConstantType::Int ? "int " : "float ")
				.append(d.name)
				.append(";");
		}

		commonHlsl.append("};");
	}

	if (!desc.dynamicValueConstants.empty())
	{
		// Constant updated every frame
		commonHlsl.append("cbuffer __D:register(b1){");
		for (const auto& d : desc.dynamicValueConstants)
		{
			commonHlsl.append(d.type == ShaderConstantType::Int ? "int " : "float ")
				.append(d.name)
				.append(";");
		}

		commonHlsl.append("};");
	}

	if (!desc.samplers.empty())
	{
		// Sampler
		for (int i = 0; i < desc.samplers.size(); ++i)
		{
			commonHlsl.append(fmt::format("SamplerState {}:register(s{});", desc.samplers[i].name, i));
		}
	}

	commonHlsl.push_back('\n');

	for (const auto& c : commons)
	{
		commonHlsl.append(c);

		if (commonHlsl.back() != '\n')
		{
			commonHlsl.push_back('\n');
		}
	}

	std::string_view token;

	std::vector<std::string > passSources(blocks.size());
	desc.passes.resize(blocks.size());

	for (size_t i = 0; i < blocks.size(); ++i)
	{
		if (ResolvePass(blocks[i], desc, passSources, commonHlsl))
		{
			return 1;
		}
	}

	// Make sure every PASS is present
	for (size_t i = 0; i < passSources.size(); ++i)
	{
		if (passSources[i].empty())
		{
			return 1;
		}
	}

	// last PASS must be output to OUTPUT
	if (!desc.passes.back().outputs.empty())
	{
		return 1;
	}

	// Compile the generated hlsl
	assert(!passSources.empty());
	//Renderer& renderer = App::GetInstance().GetRenderer();

	if (passSources.size() == 1)
	{
		if (FAILED(CompileShader(passSources[0], "__M", "ps_5_0", &desc.passes[0].cso, "Pass1", d3dinc)))
		{
			//if (!renderer.CompileShader(false, passSources[0], "__M", desc.passes[0].cso.ReleaseAndGetAddressOf(), "Pass1", &passInclude)) {
			return 1;
		}
	}
	else
	{
		// fallback to single thread
		for (size_t i = 0; i < passSources.size(); ++i)
		{
			if (FAILED(CompileShader(passSources[i], "__M", "ps_5_0", &desc.passes[i].cso, "Pass1", d3dinc)))
			{
				CLog::Log(LOGERROR,"Failed compiling passes for current scaler");
			}
		}
	}

	return 0;
}

std::vector<ShaderConstantDesc> CShaderFileLoader::GetScalerOptions(std::wstring filename)
{

	std::string source;
	std::vector<ShaderConstantDesc> constdesc;
	if (!ReadTextFile(filename.c_str(), source))
		return constdesc;

	RemoveComments(source);

	std::string_view sourceView(source);

	if (!CheckMagic(sourceView))
	{
		return constdesc;
	}

	

	size_t foundconst = source.find("//!CONSTANT");
	if (foundconst == std::string::npos)
		return constdesc;
	std::string newstring = source.substr(foundconst);
	size_t founddefault = source.find("//!DEFAULT");
	
	ShaderDesc desc;
	ResolveConstant(sourceView, desc);
	return desc.constants;
}

std::string CShaderFileLoader::GetScalerDescription(std::wstring filename)
{
	std::string source;
	if (!ReadTextFile(filename.c_str(), source))
	{
		CLog::Log(LOGERROR,"couldn't read file for scaler {}", WToA(filename).c_str());
		return false;
	}

	RemoveComments(source);
	std::string_view sourceView(source);
	std::string_view token;
	std::string scalertype, nothing;
	while (true)
	{
		if (!CheckNextToken<true>(sourceView, META_INDICATOR))
			break;
		if (GetNextToken<false>(sourceView, token))
		{
			break;
		}

		std::string t = ToUpper(token);
		if (t == "MPC")
		{
			GetNextExpr(sourceView, nothing);
		}
		else if (t == "VERSION")
		{
			UINT version;
			if (GetNextNumber(sourceView, version))
			{
				return "";
			}

			if (version != SCALERCOMPILE_VERSION)
			{
				return "";
			}

			if (GetNextToken<false>(sourceView, token) != 2)
			{
				return "";
			}
		}
		else if (t == "OUTPUT_WIDTH")
		{
			GetNextExpr(sourceView, nothing);
		}
		else if (t == "OUTPUT_HEIGHT")
		{
			GetNextExpr(sourceView, nothing);
		}
		else if (t == "SCALER_TYPE")
		{
			if (GetNextExpr(sourceView, scalertype))
			{
				return "";
			}
		}
		else if (t == "DESCRIPTION")
		{
			std::string_view descrip;
			if (GetNextString(sourceView, descrip))
			{
				return "";
			}

			std::string returndesc;
			returndesc = descrip;
			return returndesc;
		}
		else
		{
			return "";
		}
	}

	return "";
}

std::string CShaderFileLoader::GetScalerType(std::wstring filename)
{
	std::string source;
	if (!ReadTextFile(filename.c_str(), source))
	{
		CLog::Log(LOGERROR, "couldn't read file for scaler {}", WToA(filename).c_str());
		return "";
	}

	RemoveComments(source);
	std::string_view sourceView(source);
	std::string_view token;
	std::string scalertype, nothing;
	while (true)
	{
		if (!CheckNextToken<true>(sourceView, META_INDICATOR))
			break;
		if (GetNextToken<false>(sourceView, token))
		{
			break;
		}

		std::string t = ToUpper(token);
		if (t == "MPC")
		{
			GetNextExpr(sourceView, nothing);
		}
		else if (t == "VERSION")
		{
			UINT version;
			if (GetNextNumber(sourceView, version))
			{
				return "";
			}

			if (version != SCALERCOMPILE_VERSION)
			{
				return "";
			}

			if (GetNextToken<false>(sourceView, token) != 2)
			{
				return "";
			}
		}
		else if (t == "OUTPUT_WIDTH")
		{
			GetNextExpr(sourceView, nothing);
		}
		else if (t == "OUTPUT_HEIGHT")
		{
			GetNextExpr(sourceView, nothing);
		}
		else if (t == "SCALER_TYPE")
		{
			if (GetNextExpr(sourceView, scalertype))
			{
				return "";
			}

			return scalertype;
		}
		else
		{
			return "";
		}
	}

	return "";
}

bool CShaderFileLoader::Compile(ShaderDesc& desc, bool usecache)
{
	if (m_pFile == L"")
		return false;
	XFILE::CFile currentfile;
	currentfile.Exists(WToA(m_pFile));
	std::wstring currentpath = m_pFile;// GetFilePathExists(m_pFile.c_str());
	
	std::wifstream infile(currentpath.c_str());
	std::string source;
	bool cacheinit = Hasher::GetInstance().Initialize();
	if (!ReadTextFile(currentpath.c_str(), source))
	{
		CLog::Log(LOGERROR, "couldn't read for compile {}", WToA(currentpath).c_str());
		return false;
	}

	RemoveComments(source);

	std::string md5;
	if (usecache)
	{
		std::vector<BYTE> hash;
		if (!Hasher::GetInstance().Hash(source.data(), source.size(), hash)) {}
		else
		{
			md5 = Bin2Hex(hash.data(), hash.size());

			if (ShaderCache::GetInstance().Load(m_pFile.c_str(), md5, desc))
			{
				// read from cache
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
		Constant,
		Texture,
		Sampler,
		Common,
		Pass
	};

	std::string_view headerBlock;
	std::vector<std::string_view > constantBlocks;
	std::vector<std::string_view > textureBlocks;
	std::vector<std::string_view > samplerBlocks;
	std::vector<std::string_view > commonBlocks;
	std::vector<std::string_view > passBlocks;

	BlockType curBlockType = BlockType::Header;
	size_t curBlockOff = 0;

	auto completeCurrentBlock = [&](size_t len, BlockType newBlockType)
	{
		switch (curBlockType)
		{
		case BlockType::Header:
			headerBlock = sourceView.substr(curBlockOff, len);
			break;
		case BlockType::Constant:
			constantBlocks.push_back(sourceView.substr(curBlockOff, len));
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
			passBlocks.push_back(sourceView.substr(curBlockOff, len));
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

				if (blockType == "CONSTANT")
				{
					completeCurrentBlock(len, BlockType::Constant);
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
	if (passBlocks.empty())
	{
		return 1;
	}

	if (ResolveHeader(headerBlock, desc))
	{
		return 1;
	}

	for (size_t i = 0; i < constantBlocks.size(); ++i)
	{
		if (ResolveConstant(constantBlocks[i], desc))
		{
			return 1;
		}
	}

	// texture input
	ShaderIntermediateTextureDesc& inputTex = desc.textures.emplace_back();
	inputTex.name = "INPUT";
	for (size_t i = 0; i < textureBlocks.size(); ++i)
	{
		if (ResolveTexture(textureBlocks[i], desc))
		{
			return 1;
		}
	}

	for (size_t i = 0; i < samplerBlocks.size(); ++i)
	{
		if (ResolveSampler(samplerBlocks[i], desc))
		{
			return 1;
		}
	}

	{
		// Make sure there are no duplicate names
		std::unordered_set<std::string > names;
		for (const auto& d : desc.constants)
		{
			if (names.find(d.name) != names.end())
			{
				return 1;
			}

			names.insert(d.name);
		}

		for (const auto& d : desc.textures)
		{
			if (names.find(d.name) != names.end())
			{
				return 1;
			}

			names.insert(d.name);
		}

		for (const auto& d : desc.samplers)
		{
			if (names.find(d.name) != names.end())
			{
				return 1;
			}

			names.insert(d.name);
		}
	}

	for (size_t i = 0; i < commonBlocks.size(); ++i)
	{
		if (ResolveCommon(commonBlocks[i]))
		{
			return 1;
		}
	}

	if (ResolvePasses(passBlocks, commonBlocks, desc, this))
	{
		return 1;
	}

	if (usecache)
		ShaderCache::GetInstance().Save(m_pFile.c_str(), md5, desc);

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