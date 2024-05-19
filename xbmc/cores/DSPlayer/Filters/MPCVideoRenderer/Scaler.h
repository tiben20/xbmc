/*
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

class CMPCVRRenderer;

class CD3DScaler : public ID3DResource
{
public:
  CD3DScaler();
  virtual ~CD3DScaler();
  bool Create(const ShaderDesc& desc, const ShaderOption& option);
  bool IsCreated() { return m_bCreated; };
  void Release();
  void ReleaseResource();
  
  void Draw(CMPCVRRenderer* renderer);

  void OnDestroyDevice(bool fatal) override;
  void OnCreateDevice() override;

  void SetOption(ShaderOption option);

  CD3DTexture GetOutputSurface()
  {
    //the output is always the second texture in the array
    if (m_pTextures[1].Get())
      return m_pTextures[1];
  }
  void ResetOutputTexture(UINT width, UINT height,DXGI_FORMAT fmt);

private:
  bool m_bUpdateBuffer;
  bool m_bCreated;
  SmallVector<ID3D11SamplerState*> m_pSamplers;
  SmallVector<CD3DTexture> m_pTextures;
  std::vector<SmallVector<ID3D11ShaderResourceView*>> m_pSRVs;
  std::vector<SmallVector<ID3D11UnorderedAccessView*>> m_pUAVs;
  std::vector<Microsoft::WRL::ComPtr<ID3D11ComputeShader>> m_pComputeShaders;
  SmallVector<std::pair<uint32_t, uint32_t>> m_pDispatches;

  //those are only used to avoid warnings from d3d11 device
  std::vector<ID3D11ShaderResourceView*> m_pNullSRV;
  std::vector<ID3D11UnorderedAccessView*> m_pNullUAV;

  //constant buffer and options
  SmallVector<Constant32, 32> m_pConstants;
  Microsoft::WRL::ComPtr<ID3D11Buffer> m_pConstantBuffer;

  SIZE CalcOutputSize(const std::pair<std::string, std::string>& outputSizeExpr, const ShaderOption& option, SIZE scalingWndSize, SIZE inputSize, mu::Parser& exprParser);
  bool InitializeConstants(const ShaderDesc& desc, const ShaderOption& option, SIZE inputSize, SIZE outputSize);
  void ChangeConstant(Constant32 pParam, int index);
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
  void TestConsts()
  {
    if (std::holds_alternative<ShaderConstant<float>>(m_pDesc.params[0].constant))
    {
      ShaderConstant<float> constf;
      constf = std::get<ShaderConstant<float>>(m_pDesc.params[0].constant);
      
    }
    m_pScaler->SetOption(m_pOption);
  };
  void ResetOutputTexture(UINT width, UINT height, DXGI_FORMAT fmt);

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

