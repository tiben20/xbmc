﻿/*
 * Original developed by Minigraph author James Stanard
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
#include <string>
#include <vector>
#include "ShadersLoader.h"
#include "guilib/D3DResource.h"
#include "DSUtil/Geometry.h"
#include "SmallVector.h"
#include "VideoRenderers/MPCVRRenderer.h"
#pragma push_macro("_UNICODE")
#undef _UNICODE
#pragma warning(push)
#pragma warning(disable: 4310)
#include "Include/MuParser/muParser.h"
#pragma warning(push)
#pragma pop_macro("_UNICODE")
#pragma comment(lib, "muparser.lib")


__declspec(align(16)) struct CONSTANT_BUFFER_4F_4int {
  DirectX::XMFLOAT4 size0;
  DirectX::XMFLOAT4 fArg;
  DirectX::XMINT4 iArg;
  DirectX::XMFLOAT2 fInternalArg;
  DirectX::XMINT2 iInternalArg;
};

struct ScalerConfigFloat
{
  std::wstring Name;
  float Value;
};

struct ScalerConfigInt
{
  std::wstring Name;
  int Value;
};

class CMPCVRRenderer;

class CD3DScaler : public ID3DResource
{
public:
  CD3DScaler();
  virtual ~CD3DScaler();
  bool Create(const ShaderDesc& desc, const ShaderOption& option);
  void Release();
  
  void Draw(CMPCVRRenderer* renderer);

  void OnDestroyDevice(bool fatal) override;
  void OnCreateDevice() override;
  CD3DTexture GetOutputSurface()
  {
    //the output is always the second texture in the array
    if (m_pTextures[1].Get())
      return m_pTextures[1];
  }
private:

  std::string m_effectString;
  SmallVector<ID3D11SamplerState*> m_pSamplers;
  SmallVector<CD3DTexture> m_pTextures;
  std::vector<SmallVector<ID3D11ShaderResourceView*>> m_pSRVs;
  std::vector<SmallVector<ID3D11UnorderedAccessView*>> m_pUAVs;

  std::vector<ID3D11ShaderResourceView*> m_pNullSRV;
  std::vector<ID3D11UnorderedAccessView*> m_pNullUAV;
  SmallVector<Constant32, 32> m_pConstants;
  Microsoft::WRL::ComPtr<ID3D11Buffer> m_pConstantBuffer;

  std::vector<Microsoft::WRL::ComPtr<ID3D11ComputeShader>> m_pComputeShaders;

  SmallVector<std::pair<uint32_t, uint32_t>> m_pDispatches;

  SIZE CalcOutputSize(const std::pair<std::string, std::string>& outputSizeExpr, const ShaderOption& option, SIZE scalingWndSize, SIZE inputSize, mu::Parser& exprParser);
  bool InitializeConstants(const ShaderDesc& desc, const ShaderOption& option, SIZE inputSize, SIZE outputSize);
};

class CD3D11DynamicScaler
{
public:
  CD3D11DynamicScaler(std::wstring filename,bool *res);
  ~CD3D11DynamicScaler();

  void Init();
  void Unload();
  
  std::wstring GetScalerName() { return m_pFilename; }
  void SetShaderConstants(std::vector<ShaderParameterDesc> consts) { m_pDesc.params = consts; }

  void Draw(CMPCVRRenderer* renderer) { m_pScaler->Draw(renderer); };
  CD3DTexture GetOutputSurface() { return m_pScaler->GetOutputSurface(); };

  int GetNumberPasses() { return m_pDesc.passes.size(); };
private:
  CD3DScaler* m_pScaler;
  ShaderOption m_pOption = {};
  ShaderDesc m_pDesc = {};
  CD3DEffect m_effect;
  Com::SmartRect m_srcRect;
  std::wstring m_pFilename;
  Microsoft::WRL::ComPtr< ID3D11Buffer> m_pConstantBuffer;
  //std::vector<GraphicsPSO> m_pPSO;
};

