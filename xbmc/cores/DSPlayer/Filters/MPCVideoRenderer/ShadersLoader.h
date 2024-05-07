/*
 * the shader parsing coming from https://github.com/Blinue/Magpie
 *
 * (C) 2022 Ti-BEN
 *
 * This file is part of MPC-BE.
 *
 * MPC-BE is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-BE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once


#include <vector>
#include <string>
#include <rpc.h>
#include <ppl.h>
#include "ppltasks.h"
#include <map>
#include <variant>
#include <wrl/client.h>
#include <dxgiformat.h>
#include <d3d11.h>

#define SCALERCOMPILE_VERSION 1

enum class ShaderIntermediateTextureFormat {
	R8_UNORM,
	R16_UNORM,
	R16_FLOAT,
	R8G8_UNORM,
	B5G6R5_UNORM,
	R16G16_UNORM,
	R16G16_FLOAT,
	R8G8B8A8_UNORM,
	B8G8R8A8_UNORM,
	R10G10B10A2_UNORM,
	R32_FLOAT,
	R11G11B10_FLOAT,
	R32G32_FLOAT,
	R16G16B16A16_UNORM,
	R16G16B16A16_FLOAT,
	R32G32B32A32_FLOAT
};

struct ShaderIntermediateTextureDesc {
	std::pair<std::string, std::string> sizeExpr;
	ShaderIntermediateTextureFormat format = ShaderIntermediateTextureFormat::B8G8R8A8_UNORM;
	std::string name;
	std::string source;

	static const DXGI_FORMAT DXGI_FORMAT_MAP[16];
};

enum class ShaderSamplerFilterType {
	Linear,
	Point
};

enum class ShaderSamplerAddressType {
	Clamp,
	Wrap
};

struct ShaderSamplerDesc {
	ShaderSamplerFilterType filterType = ShaderSamplerFilterType::Linear;
	ShaderSamplerAddressType addressType = ShaderSamplerAddressType::Clamp;
	std::string name;
};

enum class ShaderConstantType {
	Float,
	Int
};

struct ShaderValueConstantDesc {
	std::string name;
	ShaderConstantType type = ShaderConstantType::Float;
	std::string valueExpr;
};

struct ShaderConstantDesc {
	std::string name;
	std::string label;
	ShaderConstantType type = ShaderConstantType::Float;
	std::variant<float, int> defaultValue;
	std::variant<std::monostate, float, int> minValue;
	std::variant<std::monostate, float, int> maxValue;
	std::variant<float, int> currentValue;
};

struct ShaderPassDesc {
	std::vector<UINT> inputs;
	std::vector<UINT> outputs;
	Microsoft::WRL::ComPtr<ID3DBlob> cso;
};

struct ShaderDesc {
	// The output used to calculate the effect, a null value means that any size output is supported
	std::pair<std::string, std::string> outSizeExpr;

	std::string shaderDescription;

	std::vector<ShaderConstantDesc> constants;
	std::vector<ShaderValueConstantDesc> valueConstants;
	std::vector<ShaderValueConstantDesc> dynamicValueConstants;

	std::vector<ShaderIntermediateTextureDesc> textures;
	std::vector<ShaderSamplerDesc> samplers;

	std::vector<ShaderPassDesc> passes;
};

class CShaderFileLoader : public ID3DInclude
{
public:
	CShaderFileLoader(std::wstring filename)
	{
		m_pFile = filename;
	}

	~CShaderFileLoader() {};
	bool Compile(ShaderDesc& desc,bool useCache);
	std::string GetScalerType(std::wstring filename);
	std::vector<ShaderConstantDesc> GetScalerOptions(std::wstring filename);
	std::string GetScalerDescription(std::wstring filename);

	// ID3DInclude interface
	__declspec(nothrow) HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) override;
	__declspec(nothrow) HRESULT __stdcall Close(LPCVOID pData) override;
private:
	
	std::wstring m_pFile;
	
};

