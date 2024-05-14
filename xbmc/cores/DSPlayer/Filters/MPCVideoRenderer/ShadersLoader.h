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

#include <array>
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
#include <parallel_hashmap/phmap.h>

#define SCALERCOMPILE_VERSION 4

struct ShaderIntermediateTextureFormatDesc {
	const char* name;
	DXGI_FORMAT dxgiFormat;
	uint32_t nChannel;
	const char* srvTexelType;
	const char* uavTexelType;
};

static constexpr ShaderIntermediateTextureFormatDesc FORMAT_DESCS[] = {
		{"R32G32B32A32_FLOAT", DXGI_FORMAT_R32G32B32A32_FLOAT, 4, "float4", "float4"},
		{"R16G16B16A16_FLOAT", DXGI_FORMAT_R16G16B16A16_FLOAT, 4, "float4", "float4"},
		{"R16G16B16A16_UNORM", DXGI_FORMAT_R16G16B16A16_UNORM, 4, "float4", "unorm float4"},
		{"R16G16B16A16_SNORM", DXGI_FORMAT_R16G16B16A16_SNORM, 4, "float4", "snorm float4"},
		{"R32G32_FLOAT", DXGI_FORMAT_R32G32_FLOAT, 2, "float2", "float2"},
		{"R10G10B10A2_UNORM", DXGI_FORMAT_R10G10B10A2_UNORM, 4, "float4", "unorm float4"},
		{"R11G11B10_FLOAT", DXGI_FORMAT_R11G11B10_FLOAT, 3, "float3", "float3"},
		{"R8G8B8A8_UNORM", DXGI_FORMAT_R8G8B8A8_UNORM, 4, "float4", "unorm float4"},
		{"R8G8B8A8_SNORM", DXGI_FORMAT_R8G8B8A8_SNORM, 4, "float4", "snorm float4"},
		{"R16G16_FLOAT", DXGI_FORMAT_R16G16_FLOAT, 2, "float2", "float2"},
		{"R16G16_UNORM", DXGI_FORMAT_R16G16_UNORM, 2, "float2", "unorm float2"},
		{"R16G16_SNORM", DXGI_FORMAT_R16G16_SNORM, 2, "float2", "snorm float2"},
		{"R32_FLOAT" ,DXGI_FORMAT_R32_FLOAT, 1, "float", "float"},
		{"R8G8_UNORM", DXGI_FORMAT_R8G8_UNORM, 2, "float2", "unorm float2"},
		{"R8G8_SNORM", DXGI_FORMAT_R8G8_SNORM, 2, "float2", "snorm float2"},
		{"R16_FLOAT", DXGI_FORMAT_R16_FLOAT, 1, "float", "float"},
		{"R16_UNORM", DXGI_FORMAT_R16_UNORM, 1, "float", "unorm float"},
		{"R16_SNORM", DXGI_FORMAT_R16_SNORM,1, "float", "snorm float"},
		{"R8_UNORM", DXGI_FORMAT_R8_UNORM, 1, "float", "unorm float"},
		{"R8_SNORM", DXGI_FORMAT_R8_SNORM, 1, "float", "snorm float"},
		{"UNKNOWN", DXGI_FORMAT_UNKNOWN, 4, "float4", "float4"}
};

union Constant32 {
	float floatVal;
	uint32_t uintVal;
	int intVal;
};

enum class ShaderIntermediateTextureFormat {
	R32G32B32A32_FLOAT,
	R16G16B16A16_FLOAT,
	R16G16B16A16_UNORM,
	R16G16B16A16_SNORM,
	R32G32_FLOAT,
	R10G10B10A2_UNORM,
	R11G11B10_FLOAT,
	R8G8B8A8_UNORM,
	R8G8B8A8_SNORM,
	R16G16_FLOAT,
	R16G16_UNORM,
	R16G16_SNORM,
	R32_FLOAT,
	R8G8_UNORM,
	R8G8_SNORM,
	R16_FLOAT,
	R16_UNORM,
	R16_SNORM,
	R8_UNORM,
	R8_SNORM,
	UNKNOWN
};

struct ShaderIntermediateTextureDesc {
	std::pair<std::string, std::string> sizeExpr;
	ShaderIntermediateTextureFormat format = ShaderIntermediateTextureFormat::UNKNOWN;
	std::string name;
	std::string source;
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

template <typename T>
struct ShaderConstant {
	T defaultValue;
	T minValue;
	T maxValue;
	T step;
};

struct ShaderParameterDesc {
	std::string name;
	std::string label;
	std::variant<ShaderConstant<float>, ShaderConstant<int>> constant;
};

//should we use smallvector from magpie
struct ShaderPassDesc {
	Microsoft::WRL::ComPtr<ID3DBlob> cso;
	std::vector<uint32_t> inputs;
	std::vector<uint32_t> outputs;
	std::array<uint32_t, 3> numThreads{};
	std::pair<uint32_t, uint32_t> blockSize{};
	std::string desc;
	bool isPSStyle = false;
};

struct ShaderFlags {
	// Input
	static constexpr const uint32_t InlineParams = 1;
	static constexpr const uint32_t FP16 = 1 << 1;
	// output
	// This shader requires frame number and mouse position
	static constexpr const uint32_t UseDynamic = 1 << 4;
};

struct ShaderDesc {
	std::string name;
	std::string sortName;	// For UI use only

	const std::pair<std::string, std::string>& GetOutputSizeExpr() const noexcept {
		return textures[1].sizeExpr;
	}

	std::vector<ShaderParameterDesc> params;
	// 0: INPUT
	// 1: OUTPUT
	// > 1: intermediate texture
	std::vector<ShaderIntermediateTextureDesc> textures;
	std::vector<ShaderSamplerDesc> samplers;
	std::vector<ShaderPassDesc> passes;

	uint32_t flags = 0;	// ShaderFlags
};

struct ShaderCompilerFlags {
	static constexpr const uint32_t NoCache = 1;
	static constexpr const uint32_t SaveSources = 1 << 1;
	static constexpr const uint32_t WarningsAreErrors = 1 << 2;
	static constexpr const uint32_t NoCompile = 1 << 3;
};

enum class ScalingType {
	Normal, // Scale represents the zoom factor
	Fit, // Scale represents the ratio relative to the maximum proportional scaling that the screen can accommodate
	Absolute, // Scale represents the target size (unit is pixels)
	Fill // Fill the screen, the Scale parameter is not used at this time
};

struct ShaderOption {
	std::wstring name;
	phmap::flat_hash_map<std::wstring, float> parameters;
	ScalingType scalingType = ScalingType::Normal;
	std::pair<float, float> scale = { 1.0f,1.0f };
	uint32_t flags = 0;	// EffectOptionFlags

	bool HasScale() const noexcept {
		return scalingType != ScalingType::Normal ||
			std::abs(scale.first - 1.0f) > 1e-5 || std::abs(scale.second - 1.0f) > 1e-5;
	}
};

class CShaderFileLoader : public ID3DInclude
{
public:
	CShaderFileLoader(std::wstring filename)
	{
		m_pFile = filename;
	}

	~CShaderFileLoader() {};
	uint32_t Compile(ShaderDesc& desc, uint32_t flags, const phmap::flat_hash_map<std::wstring, float>* inlineParams);
	std::string GetScalerType(std::wstring filename);
	std::vector<ShaderParameterDesc> GetScalerOptions(std::wstring filename);
	std::string GetScalerDescription(std::wstring filename);

	// ID3DInclude interface
	__declspec(nothrow) HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) override;
	__declspec(nothrow) HRESULT __stdcall Close(LPCVOID pData) override;
private:
	
	std::wstring m_pFile;
	
};

