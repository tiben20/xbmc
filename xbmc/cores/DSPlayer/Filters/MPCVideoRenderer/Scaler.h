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
class CD3DDSShader : public ID3DResource
{
public:
  CD3DDSShader();
  virtual ~CD3DDSShader();
  bool Create(const ShaderDesc& desc, const ShaderOption& option);
  void Release();
  bool SetFloatArray(LPCSTR handle, const float* val, unsigned int count);
  bool SetMatrix(LPCSTR handle, const float* mat);
  bool SetTechnique(LPCSTR handle);
  bool SetTexture(LPCSTR handle, CD3DTexture& texture);
  bool SetResources(LPCSTR handle, ID3D11ShaderResourceView** ppSRViews, size_t count);
  bool SetConstantBuffer(LPCSTR handle, ID3D11Buffer* buffer);
  bool SetScalar(LPCSTR handle, float value);
  bool Begin(UINT* passes, DWORD flags);
  bool BeginPass(UINT pass);
  bool EndPass();
  bool End();
  
  void Draw(CMPCVRRenderer* renderer);

  ID3DX11Effect* Get() const { return m_effect.Get(); }

  void OnDestroyDevice(bool fatal) override;
  void OnCreateDevice() override;

private:

  std::string m_effectString;
  DefinesMap m_defines;
  Microsoft::WRL::ComPtr<ID3DX11Effect> m_effect;
  Microsoft::WRL::ComPtr<ID3DX11EffectTechnique> m_techniquie;
  Microsoft::WRL::ComPtr<ID3DX11EffectPass> m_currentPass;

  SmallVector<ID3D11SamplerState*> _samplers;
  SmallVector<CD3DTexture> _textures;
  std::vector<SmallVector<ID3D11ShaderResourceView*>> _srvs;
  // 后半部分为空，用于解绑
  std::vector<SmallVector<ID3D11UnorderedAccessView*>> _uavs;

  SmallVector<Constant32, 32> _constants;
  Microsoft::WRL::ComPtr<ID3D11Buffer> m_pConstantBuffer;

  std::vector<Microsoft::WRL::ComPtr<ID3D11ComputeShader>> _shaders;

  SmallVector<std::pair<uint32_t, uint32_t>> _dispatches;

  SIZE CalcOutputSize(const std::pair<std::string, std::string>& outputSizeExpr, const ShaderOption& option, SIZE scalingWndSize, SIZE inputSize, mu::Parser& exprParser);
  bool InitializeConstants(const ShaderDesc& desc, const ShaderOption& option, SIZE inputSize, SIZE outputSize);
};

class CD3D11Scaler
{
public:
  CD3D11Scaler(std::wstring name);
  ~CD3D11Scaler();
  

  std::wstring Name() { return m_pName; }

  std::vector<ScalerConfigFloat> g_ScalerInternalFloat;
  std::vector<ScalerConfigInt> g_ScalerInternalInt;
  /*void AddConfig(std::wstring Name, int Value, int MinValue = 0, int MaxValue = 0, int increment = 0)
  {
    m_pScalerConfigInt.push_back(ScalerConfigInt{ Name,Value,MinValue,MaxValue,increment });
  }
  void AddConfig(std::wstring Name, float Value, float MinValue = 0.0f, float MaxValue = 0.0f, float increment = 0.0f)
  {
    m_pScalerConfigFloat.push_back(ScalerConfigFloat{ Name,Value,MinValue,MaxValue,increment });
  }

  void AddBufferConstant(ScalerConfigInt cfg) { m_pScalerConfigInt.push_back(cfg); };
  void AddBufferConstant(ScalerConfigFloat cfg) { m_pScalerConfigFloat.push_back(cfg); };*/
  float GetConfigFloat(std::wstring name);
  int GetConfigInt(std::wstring name);
  void SetConfigFloat(std::wstring name, float value);
  void SetConfigInt(std::wstring name, int value);

  //void ShaderPass(GraphicsContext& Context, ColorBuffer& dest, ColorBuffer& source, int w, int h, int iArgs[4], float fArgs[4]);
  void Done() { m_bFirstPass = true; }
  void CreateTexture(std::wstring name, Com::SmartRect rect, DXGI_FORMAT fmt);
  void CreateDynTextureFromDDS(std::wstring texture, Com::SmartRect rect, DXGI_FORMAT fmt, std::string ddsfile);
  void CreateDynTexture(std::wstring name, Com::SmartRect rect, DXGI_FORMAT fmt);

  CD3DTexture GetDynTexture(int index) { return m_pScalingTextureDyn[index]; }

  //void SetTextureSrv(GraphicsContext& Context, std::wstring name, int index, int table, bool setResourceState = true);
  //void SetDynTextureSrv(GraphicsContext& Context, std::vector<UINT> idx,int table, ColorBuffer& srcInputBuffer, bool setResourceState = true);
  //void SetRenderTargets(GraphicsContext& Context, std::vector<std::wstring> targets, bool setResourceState=false);
  //void SetDynRenderTargets(GraphicsContext& Context, std::vector<UINT> targets, bool setResourceState = false);

  bool         g_bTextureCreated = false;

  void FreeTexture();
  void FreeDynTexture();
private:
  std::wstring m_pName;
  bool         m_bFirstPass = true;
  
  std::vector<CD3DTexture> m_pScalingTextureDyn;
  std::map<std::wstring, CD3DTexture> m_pScalingTexture;
  
  /*std::vector<ScalerConfigInt> m_pScalerConfigInt;
  std::vector<ScalerConfigFloat> m_pScalerConfigFloat;*/
  CONSTANT_BUFFER_4F_4int m_pConstantBuffer;
};


class CD3D11DynamicScaler
{
public:
  CD3D11DynamicScaler(std::wstring filename,bool *res);
  ~CD3D11DynamicScaler();

  void Init(DXGI_FORMAT srcfmt, Com::SmartRect src, Com::SmartRect dst);
  void Init();
  void Render(Com::SmartRect dstrect, CD3DTexture& dest, CD3DTexture& source);
  void Unload();
  
  std::wstring GetScalerName() { return m_pFilename; }
  void SetShaderConstants(std::vector<ShaderParameterDesc> consts) { m_pDesc.params = consts; }

  void Draw(CMPCVRRenderer* renderer) { m_pScaler->Draw(renderer); };
private:
  CD3DDSShader* m_pScaler;
  ShaderOption m_pOption = {};
  ShaderDesc m_pDesc = {};
  CD3DEffect m_effect;
  Com::SmartRect m_srcRect;
  std::wstring m_pFilename;
  Microsoft::WRL::ComPtr< ID3D11Buffer> m_pConstantBuffer;
  //std::vector<GraphicsPSO> m_pPSO;
};

