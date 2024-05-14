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

#include "stdafx.h"
#include "scaler.h"
#include "dxgiformat.h"
#include "filesystem/file.h"
#include "utils/log.h"
#include "rendering/dx/DeviceResources.h"
#include "rendering/dx/RenderContext.h"
#include "VideoRenderers/MPCVRRenderer.h"
#include "utils/CharsetConverter.h"

CD3D11Scaler::CD3D11Scaler(std::wstring name)
{
  m_pName = name;
  m_pConstantBuffer = {};
}

CD3D11Scaler::~CD3D11Scaler()
{
  
}

DirectX::XMFLOAT4 CreateFloat4(int width, int height)
{
  float x = (float)width;
  float y = (float)height;
  float z = (float)1 / width;
  float w = (float)1 / height;
  return { x,y,z,w };
}

const DXGI_FORMAT DXGI_FORMAT_MAPPING[16] = {
  DXGI_FORMAT_R8_UNORM,
  DXGI_FORMAT_R16_UNORM,
  DXGI_FORMAT_R16_FLOAT,
  DXGI_FORMAT_R8G8_UNORM,
  DXGI_FORMAT_B5G6R5_UNORM,
  DXGI_FORMAT_R16G16_UNORM,
  DXGI_FORMAT_R16G16_FLOAT,
  DXGI_FORMAT_R8G8B8A8_UNORM,
  DXGI_FORMAT_B8G8R8A8_UNORM,
  DXGI_FORMAT_R10G10B10A2_UNORM,
  DXGI_FORMAT_R32_FLOAT,
  DXGI_FORMAT_R11G11B10_FLOAT,
  DXGI_FORMAT_R32G32_FLOAT,
  DXGI_FORMAT_R16G16B16A16_UNORM,
  DXGI_FORMAT_R16G16B16A16_FLOAT,
  DXGI_FORMAT_R32G32B32A32_FLOAT
};

CD3DDSShader::CD3DDSShader()
{
  m_effect = nullptr;
  m_techniquie = nullptr;
  m_currentPass = nullptr;
}

CD3DDSShader::~CD3DDSShader()
{
  Release();
}

bool CD3DDSShader::Create(const ShaderDesc& desc, const ShaderOption& option)
{
  SIZE inputSize, outputSize;
  inputSize = { (LONG)DX::Windowing()->GetBackBuffer().GetWidth(), (LONG)DX::Windowing()->GetBackBuffer().GetHeight() };
  outputSize = { (LONG)DX::Windowing()->GetBackBuffer().GetWidth(), (LONG)DX::Windowing()->GetBackBuffer().GetHeight() };
  static mu::Parser exprParser;
  exprParser.DefineConst("INPUT_WIDTH", inputSize.cx);
  exprParser.DefineConst("INPUT_HEIGHT", inputSize.cy);
  const SIZE scalingWndSize = outputSize;

  exprParser.DefineConst("OUTPUT_WIDTH", outputSize.cx);
  exprParser.DefineConst("OUTPUT_HEIGHT", outputSize.cy);
  _samplers.resize(desc.samplers.size());
  for (UINT i = 0; i < _samplers.size(); ++i) {
    const ShaderSamplerDesc& samDesc = desc.samplers[i];
    _samplers[i] = CMPCVRRenderer::Get()->GetSampler(
      samDesc.filterType == ShaderSamplerFilterType::Linear ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT,
      samDesc.addressType == ShaderSamplerAddressType::Clamp ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP
    );

    if (!_samplers[i]) {
      CLog::Log(LOGERROR,"Failed to create sampler{}", samDesc.name);
      return false;
    }
  }

  // 创建中间纹理
  // 第一个为 INPUT，第二个为 OUTPUT
  _textures.resize(desc.textures.size());
  
  _textures[0] = CMPCVRRenderer::Get()->GetIntermediateTarget();

  // 创建输出纹理，格式始终是 DXGI_FORMAT_R8G8B8A8_UNORM
  _textures[1].Create(outputSize.cx, outputSize.cy, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, FORMAT_DESCS[(uint32_t)desc.textures[1].format].dxgiFormat);
    /*= DirectXHelper::CreateTexture2D(
    DX::DeviceResources::GetD3DDevice(),
    FORMAT_DESCS[(uint32_t)desc.textures[1].format].dxgiFormat,
    outputSize.cx,
    outputSize.cy,
    D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS
  );

  *inOutTexture = _textures[1].get();
  if (!*inOutTexture) {
    CLog::Log(LOGERROR,"创建输出纹理失败");
    return false;
  }*/

  for (size_t i = 2; i < desc.textures.size(); ++i) {
    const ShaderIntermediateTextureDesc& texDesc = desc.textures[i];

    if (!texDesc.source.empty()) {
      // Load texture from file
#if TODO
      size_t delimPos = desc.name.find_last_of('\\');

      std::string texPath = delimPos == std::string::npos
        ? StrUtils::Concat("effects\\", texDesc.source)
        : StrUtils::Concat("effects\\", std::string_view(desc.name.c_str(), delimPos + 1), texDesc.source);
      _textures[i] = TextureLoader::Load(
        StrUtils::UTF8ToUTF16(texPath).c_str(), deviceResources.GetD3DDevice());
      if (!_textures[i]) {
        CLog::Log(LOGERROR,fmt::format("Loading texture {} failed", texDesc.source));
        return false;
      }

      if (texDesc.format != ShaderIntermediateTextureFormat::UNKNOWN) {
        // Check if texture format matches
        D3D11_TEXTURE2D_DESC srcDesc{};
        _textures[i]->GetDesc(&srcDesc);
        if (srcDesc.Format != FORMAT_DESCS[(uint32_t)texDesc.format].dxgiFormat) {
          CLog::Log(LOGERROR,"SOURCE Texture format mismatch");
          return false;
        }
      }
#endif
    }
    else {
      SIZE texSize{};
      try {
        exprParser.SetExpr(texDesc.sizeExpr.first);
        texSize.cx = std::lround(exprParser.Eval());
        exprParser.SetExpr(texDesc.sizeExpr.second);
        texSize.cy = std::lround(exprParser.Eval());
      }
      catch (const mu::ParserError& e) {
        CLog::Log(LOGERROR,fmt::format("Computation of intermediate texture size {} failed: {}", e.GetExpr(), e.GetMsg()));
        return false;
      }

      if (texSize.cx <= 0 || texSize.cy <= 0) {
        CLog::Log(LOGERROR,"Illegal intermediate texture size");
        return false;
      }

      if (!_textures[i].Create(texSize.cx, texSize.cy, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, FORMAT_DESCS[(UINT)texDesc.format].dxgiFormat))
      {
        CLog::Log(LOGERROR, "Failed to create texture");
        return false;
      }
        /*= DirectXHelper::CreateTexture2D(
        deviceResources.GetD3DDevice(),
        ShaderHelper::FORMAT_DESCS[(UINT)texDesc.format].dxgiFormat,
        texSize.cx,
        texSize.cy,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS
      );
      if (!_textures[i]) {
        CLog::Log(LOGERROR,"创建纹理失败");
        return false;
      }*/
    }
  }

  _shaders.resize(desc.passes.size());
  _srvs.resize(desc.passes.size());
  _uavs.resize(desc.passes.size());
  for (UINT i = 0; i < _shaders.size(); ++i) {
    const ShaderPassDesc& passDesc = desc.passes[i];
#if 1
    HRESULT hr = DX::DeviceResources::Get()->GetD3DDevice()->CreateComputeShader(
      passDesc.cso->GetBufferPointer(), passDesc.cso->GetBufferSize(), nullptr, _shaders[i].ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
      CLog::Log(LOGERROR, "Failed to create compute shader");
      return false;
    }

    _srvs[i].resize(passDesc.inputs.size());
    for (UINT j = 0; j < passDesc.inputs.size(); ++j) {
      auto srv = _srvs[i][j] = CMPCVRRenderer::Get()->GetShaderResourceView(_textures[passDesc.inputs[j]].Get());
      if (!srv) {
        CLog::Log(LOGERROR, "GetShaderResourceView failed");
        return false;
      }
    }

    _uavs[i].resize(passDesc.outputs.size() * 2);
    for (UINT j = 0; j < passDesc.outputs.size(); ++j) {
      auto uav = _uavs[i][j] = CMPCVRRenderer::Get()->GetUnorderedAccessView(_textures[passDesc.outputs[j]].Get());
      if (!uav) {
        CLog::Log(LOGERROR, "GetUnorderedAccessView failed");
        return false;
      }
    }
#else
    HRESULT hr = deviceResources.GetD3DDevice()->CreateComputeShader(
      passDesc.cso->GetBufferPointer(), passDesc.cso->GetBufferSize(), nullptr, _shaders[i].put());
    if (FAILED(hr)) {
      Logger::Get().ComError("创建计算着色器失败", hr);
      return false;
    }

    _srvs[i].resize(passDesc.inputs.size());
    for (UINT j = 0; j < passDesc.inputs.size(); ++j) {
      auto srv = _srvs[i][j] = descriptorStore.GetShaderResourceView(_textures[passDesc.inputs[j]].get());
      if (!srv) {
        CLog::Log(LOGERROR,"GetShaderResourceView 失败");
        return false;
      }
    }

    _uavs[i].resize(passDesc.outputs.size() * 2);
    for (UINT j = 0; j < passDesc.outputs.size(); ++j) {
      auto uav = _uavs[i][j] = descriptorStore.GetUnorderedAccessView(_textures[passDesc.outputs[j]].get());
      if (!uav) {
        CLog::Log(LOGERROR,"GetUnorderedAccessView 失败");
        return false;
      }
    }
#endif
    D3D11_TEXTURE2D_DESC outputDesc;
    _textures[passDesc.outputs[0]].GetDesc(&outputDesc);
    _dispatches.emplace_back(
      (outputDesc.Width + passDesc.blockSize.first - 1) / passDesc.blockSize.first,
      (outputDesc.Height + passDesc.blockSize.second - 1) / passDesc.blockSize.second
    );
  }

  if (!InitializeConstants(desc, option, inputSize, outputSize)) {
    CLog::Log(LOGERROR,"_InitializeConstants 失败");
    return false;
  }

  return true;
}


void CD3DDSShader::Release()
{
  Unregister();
  OnDestroyDevice(false);
}

void CD3DDSShader::OnDestroyDevice(bool fatal)
{
  m_effect = nullptr;
  m_techniquie = nullptr;
  m_currentPass = nullptr;
}

void CD3DDSShader::OnCreateDevice()
{
  
}

SIZE CD3DDSShader::CalcOutputSize(const std::pair<std::string, std::string>& outputSizeExpr, const ShaderOption& option, SIZE scalingWndSize, SIZE inputSize, mu::Parser& exprParser)
{
  SIZE outputSize{};

  if (outputSizeExpr.first.empty()) {
    switch (option.scalingType) {
    case ScalingType::Normal:
    {
      outputSize.cx = std::lroundf(inputSize.cx * option.scale.first);
      outputSize.cy = std::lroundf(inputSize.cy * option.scale.second);
      break;
    }
    case ScalingType::Fit:
    {
      const float fillScale = std::min(
        float(scalingWndSize.cx) / inputSize.cx,
        float(scalingWndSize.cy) / inputSize.cy
      );
      outputSize.cx = std::lroundf(inputSize.cx * fillScale * option.scale.first);
      outputSize.cy = std::lroundf(inputSize.cy * fillScale * option.scale.second);
      break;
    }
    case ScalingType::Absolute:
    {
      outputSize.cx = std::lroundf(option.scale.first);
      outputSize.cy = std::lroundf(option.scale.second);
      break;
    }
    case ScalingType::Fill:
    {
      outputSize = scalingWndSize;
      break;
    }
    default:
      assert(false);
      break;
    }
  }
  else {
    assert(!outputSizeExpr.second.empty());

    try {
      exprParser.SetExpr(outputSizeExpr.first);
      outputSize.cx = std::lround(exprParser.Eval());

      exprParser.SetExpr(outputSizeExpr.second);
      outputSize.cy = std::lround(exprParser.Eval());
    }
    catch (const mu::ParserError& e) {
      CLog::Log(LOGERROR,"Parser error:{} : {}", e.GetExpr(), e.GetMsg());
      return {};
    }
  }

  return outputSize;
}

bool CD3DDSShader::InitializeConstants(const ShaderDesc& desc, const ShaderOption& option, SIZE inputSize, SIZE outputSize)
{
  const bool isInlineParams = desc.flags & ShaderFlags::InlineParams;

  // size must be a multiple of 4
  const size_t builtinConstantCount = 10;
  size_t psStylePassParams = 0;
  for (UINT i = 0, end = (UINT)desc.passes.size() - 1; i < end; ++i) {
    if (desc.passes[i].isPSStyle) {
      psStylePassParams += 4;
    }
  }
  _constants.resize((builtinConstantCount + psStylePassParams + (isInlineParams ? 0 : desc.params.size()) + 3) / 4 * 4);
  // cbuffer __CB1 : register(b0) {
  //     uint2 __inputSize;
  //     uint2 __outputSize;
  //     float2 __inputPt;
  //     float2 __outputPt;
  //     float2 __scale;
  //     [PARAMETERS...]
  // );
  _constants[0].uintVal = inputSize.cx;
  _constants[1].uintVal = inputSize.cy;
  _constants[2].uintVal = outputSize.cx;
  _constants[3].uintVal = outputSize.cy;
  _constants[4].floatVal = 1.0f / inputSize.cx;
  _constants[5].floatVal = 1.0f / inputSize.cy;
  _constants[6].floatVal = 1.0f / outputSize.cx;
  _constants[7].floatVal = 1.0f / outputSize.cy;
  _constants[8].floatVal = outputSize.cx / (FLOAT)inputSize.cx;
  _constants[9].floatVal = outputSize.cy / (FLOAT)inputSize.cy;

  // PS 样式的通道需要的参数
  Constant32* pCurParam = _constants.data() + builtinConstantCount;
  if (psStylePassParams > 0) {
    for (UINT i = 0, end = (UINT)desc.passes.size() - 1; i < end; ++i) {
      if (desc.passes[i].isPSStyle) {
        D3D11_TEXTURE2D_DESC outputDesc;
        _textures[desc.passes[i].outputs[0]].GetDesc(&outputDesc);
        pCurParam->uintVal = outputDesc.Width;
        ++pCurParam;
        pCurParam->uintVal = outputDesc.Height;
        ++pCurParam;
        pCurParam->floatVal = 1.0f / outputDesc.Width;
        ++pCurParam;
        pCurParam->floatVal = 1.0f / outputDesc.Height;
        ++pCurParam;
      }
    }
  }

  if (!isInlineParams) {
    for (UINT i = 0; i < desc.params.size(); ++i) {
      const auto& paramDesc = desc.params[i];
      
      auto it = option.parameters.find(CCharsetConverter::UTF8ToUTF16(paramDesc.name));

      if (paramDesc.constant.index() == 0) {
        const ShaderConstant<float>& constant = std::get<0>(paramDesc.constant);
        float value = constant.defaultValue;

        if (it != option.parameters.end()) {
          value = it->second;

          if (value < constant.minValue || value > constant.maxValue)
          {
            CLog::Log(LOGERROR, "The value of parameter {} is illegal", paramDesc.name);
            return false;
          }
        }

        pCurParam->floatVal = value;
      }
      else {
        const ShaderConstant<int>& constant = std::get<1>(paramDesc.constant);
        int value = constant.defaultValue;

        if (it != option.parameters.end()) {
          value = (int)std::lroundf(it->second);

          if ((value < constant.minValue) || (value > constant.maxValue)) {
            CLog::Log(LOGERROR, "The value of parameter {} is illegal", paramDesc.name);
            return false;
          }
        }

        pCurParam->intVal = value;
      }

      ++pCurParam;
    }
  }

  D3D11_BUFFER_DESC bd{};
  bd.ByteWidth = 4 * (UINT)_constants.size();
  bd.Usage = D3D11_USAGE_DEFAULT;
  bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  

  D3D11_SUBRESOURCE_DATA initData{};
  initData.pSysMem = _constants.data();

  
  HRESULT hr = DX::DeviceResources::Get()->GetD3DDevice()->CreateBuffer(&bd, &initData, m_pConstantBuffer.ReleaseAndGetAddressOf());
  if (FAILED(hr))
  {
    CLog::Log(LOGERROR, "CreateBuffer failed");
    return false;
  }

  return true;
}

bool CD3DDSShader::SetFloatArray(LPCSTR handle, const float* val, unsigned int count)
{
  if (m_effect)
  {
    return S_OK == m_effect->GetVariableByName(handle)->SetRawValue(val, 0, sizeof(float) * count);
  }
  return false;
}

bool CD3DDSShader::SetMatrix(LPCSTR handle, const float* mat)
{
  if (m_effect)
  {
    return S_OK == m_effect->GetVariableByName(handle)->AsMatrix()->SetMatrix(mat);
  }
  return false;
}

bool CD3DDSShader::SetTechnique(LPCSTR handle)
{
  if (m_effect)
  {
    m_techniquie = m_effect->GetTechniqueByName(handle);
    if (!m_techniquie->IsValid())
      m_techniquie = nullptr;

    return nullptr != m_techniquie;
  }
  return false;
}

bool CD3DDSShader::SetTexture(LPCSTR handle, CD3DTexture& texture)
{
  if (m_effect)
  {
    ID3DX11EffectShaderResourceVariable* var = m_effect->GetVariableByName(handle)->AsShaderResource();
    if (var->IsValid())
      return SUCCEEDED(var->SetResource(texture.GetShaderResource()));
  }
  return false;
}

bool CD3DDSShader::SetResources(LPCSTR handle, ID3D11ShaderResourceView** ppSRViews, size_t count)
{
  if (m_effect)
  {
    ID3DX11EffectShaderResourceVariable* var = m_effect->GetVariableByName(handle)->AsShaderResource();
    if (var->IsValid())
      return SUCCEEDED(var->SetResourceArray(ppSRViews, 0, count));
  }
  return false;
}

bool CD3DDSShader::SetConstantBuffer(LPCSTR handle, ID3D11Buffer* buffer)
{
  if (m_effect)
  {
    ID3DX11EffectConstantBuffer* effectbuffer = m_effect->GetConstantBufferByName(handle);
    if (effectbuffer->IsValid())
      return (S_OK == effectbuffer->SetConstantBuffer(buffer));
  }
  return false;
}

bool CD3DDSShader::SetScalar(LPCSTR handle, float value)
{
  if (m_effect)
  {
    ID3DX11EffectScalarVariable* scalar = m_effect->GetVariableByName(handle)->AsScalar();
    if (scalar->IsValid())
      return (S_OK == scalar->SetFloat(value));
  }

  return false;
}

bool CD3DDSShader::Begin(UINT* passes, DWORD flags)
{
  if (m_effect && m_techniquie)
  {
    D3DX11_TECHNIQUE_DESC desc = {};
    HRESULT hr = m_techniquie->GetDesc(&desc);
    *passes = desc.Passes;
    return S_OK == hr;
  }
  return false;
}

bool CD3DDSShader::BeginPass(UINT pass)
{
  if (m_effect && m_techniquie)
  {
    m_currentPass = m_techniquie->GetPassByIndex(pass);
    if (!m_currentPass || !m_currentPass->IsValid())
    {
      m_currentPass = nullptr;
      return false;
    }
    return (S_OK == m_currentPass->Apply(0, DX::DeviceResources::Get()->GetD3DContext()));
  }
  return false;
}

bool CD3DDSShader::EndPass()
{
  if (m_effect && m_currentPass)
  {
    m_currentPass = nullptr;
    return true;
  }
  return false;
}

bool CD3DDSShader::End()
{
  if (m_effect && m_techniquie)
  {
    m_techniquie = nullptr;
    return true;
  }
  return false;
}

void CD3DDSShader::Draw(CMPCVRRenderer* renderer)
{
  ID3D11Buffer* t = m_pConstantBuffer.Get();
  DX::DeviceResources::Get()->GetD3DContext()->CSSetConstantBuffers(0, 1, &t);
  DX::DeviceResources::Get()->GetD3DContext()->CSSetSamplers(0, (UINT)_samplers.size(), _samplers.data());
    for (uint32_t i = 0; i < _dispatches.size(); ++i)
    {
      DX::DeviceResources::Get()->GetD3DContext()->CSSetShader(_shaders[i].Get(), nullptr, 0);

      DX::DeviceResources::Get()->GetD3DContext()->CSSetShaderResources(0, (UINT)_srvs[i].size(), _srvs[i].data());
      UINT uavCount = (UINT)_uavs[i].size() / 2;
      DX::DeviceResources::Get()->GetD3DContext()->CSSetUnorderedAccessViews(0, uavCount, _uavs[i].data(), nullptr);

      DX::DeviceResources::Get()->GetD3DContext()->Dispatch(_dispatches[i].first, _dispatches[i].second, 1);

      DX::DeviceResources::Get()->GetD3DContext()->CSSetUnorderedAccessViews(0, uavCount, _uavs[i].data() + uavCount, nullptr);
      renderer->OnEndPass();
    }

}


void CD3D11Scaler::CreateDynTextureFromDDS(std::wstring texture, Com::SmartRect rect, DXGI_FORMAT fmt, std::string ddsfile)
{
  /*
  assert(0);
  ColorBuffer buf;
  if (m_pScalingTextureDyn.size() == 0)
    m_pScalingTextureDyn.push_back(buf);
  buf.Create(texture, rect.Width(), rect.Height(), 1, fmt);
  m_pScalingTextureDyn.push_back(buf);
  */
}

void CD3D11Scaler::CreateDynTexture(std::wstring texture, Com::SmartRect rect, DXGI_FORMAT fmt)
{
  
  CD3DTexture buf;
  if (m_pScalingTextureDyn.size() == 0)
    m_pScalingTextureDyn.push_back(buf);
  buf.Create(rect.Width(), rect.Height(), 1, D3D11_USAGE_DYNAMIC, fmt);
  m_pScalingTextureDyn.push_back(buf);
}


void CD3D11Scaler::CreateTexture(std::wstring name, Com::SmartRect rect, DXGI_FORMAT fmt)
{
  CD3DTexture buf;
  buf.Create(rect.Width(), rect.Height(), 1, D3D11_USAGE_DYNAMIC,fmt);
  m_pScalingTexture.insert({ name,buf });
}

void CD3D11Scaler::FreeTexture()
{
  
  for (std::map<std::wstring, CD3DTexture>::iterator it = m_pScalingTexture.begin(); it != m_pScalingTexture.end(); it++)
  {
    it->second.Release();
  }
  
}

void CD3D11Scaler::FreeDynTexture()
{
  
  for (std::vector<CD3DTexture>::iterator it = m_pScalingTextureDyn.begin(); it != m_pScalingTextureDyn.end(); it++)
  {
    it->Release();
  }
  
}
/*
void CD3D11Scaler::SetDynRenderTargets(GraphicsContext& Context, std::vector<UINT> targets, bool setResourceState)
{
 
  std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> RTVs;
  RTVs.resize(targets.size());
  for (UINT x = 0; x < targets.size(); x++)
  {
    if (setResourceState)
      Context.TransitionResource(m_pScalingTextureDyn[targets[x]], D3D12_RESOURCE_STATE_RENDER_TARGET);
    RTVs.at(x) = m_pScalingTextureDyn[targets[x]].GetRTV();
  }

  //RTVs[0] = m_pScalingTexture[targets.at(0)].GetRTV();
  //RTVs[1] = m_pScalingTexture[targets.at(1)].GetRTV();
  Context.SetRenderTargets(targets.size(), RTVs.data());

}
  

void CD3D11Scaler::SetRenderTargets(GraphicsContext& Context, std::vector<std::wstring> targets, bool setResourceState)
{
  
  std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> RTVs;
  RTVs.resize(targets.size());
  for (int x = 0; x < targets.size(); x++)
  {
    if (setResourceState)
      Context.TransitionResource(m_pScalingTexture[targets.at(x)], D3D12_RESOURCE_STATE_RENDER_TARGET);
    RTVs.at(x) = m_pScalingTexture[targets.at(x)].GetRTV();
  }

  //RTVs[0] = m_pScalingTexture[targets.at(0)].GetRTV();
  //RTVs[1] = m_pScalingTexture[targets.at(1)].GetRTV();
  Context.SetRenderTargets(targets.size(), RTVs.data());
}
void CD3D11Scaler::SetTextureSrv(GraphicsContext& Context, std::wstring name, int index, int table, bool setResourceState)
{
  Context.TransitionResource(m_pScalingTexture[name], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  Context.SetDynamicDescriptor(index, table, m_pScalingTexture[name].GetSRV());
}

void CD3D11Scaler::SetDynTextureSrv(GraphicsContext& Context, std::vector<UINT> idx, int root_index, ColorBuffer& srcInputBuffer, bool setResourceState)
{
  int curidx = 0;
  for (UINT x : idx)
  {
    if (x == 0)
    {
      //sometimes the source is used in passes after the first pass
      //Cant put source state when source is user as render target
      //Context.TransitionResource(srcInputBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      Context.SetDynamicDescriptor(0, curidx, srcInputBuffer.GetSRV());
    }
    else
    {
      Context.TransitionResource(m_pScalingTextureDyn[x], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      Context.SetDynamicDescriptor(0, curidx, m_pScalingTextureDyn[x].GetSRV());
    }
    curidx += 1;
  }
}

void CD3D11Scaler::ShaderPass(GraphicsContext& Context, ColorBuffer& dest, ColorBuffer& source, int w, int h, int iArgs[4],float fArgs[4])
{
  //pipeline state and root signature should already be set
  // just removed
  //Context.TransitionResource(dest, D3D12_RESOURCE_STATE_RENDER_TARGET);
  //Context.TransitionResource(source, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


  m_pConstantBuffer.size0 = CreateFloat4(w, h);
  m_pConstantBuffer.fArg.x = fArgs[0];
  m_pConstantBuffer.fArg.y = fArgs[1];
  m_pConstantBuffer.fArg.z = fArgs[2];
  m_pConstantBuffer.fArg.w = fArgs[3];
  m_pConstantBuffer.iArg.x = iArgs[0];
  m_pConstantBuffer.iArg.y = iArgs[1];
  m_pConstantBuffer.iArg.z = iArgs[2];
  m_pConstantBuffer.iArg.w = iArgs[3];
  m_pConstantBuffer.fInternalArg.x = g_ScalerInternalFloat.size() > 0 ? g_ScalerInternalFloat[0].Value : 0.0f;
  m_pConstantBuffer.fInternalArg.y = g_ScalerInternalFloat.size() > 0 ? g_ScalerInternalFloat[1].Value : 0.0f;
  m_pConstantBuffer.iInternalArg.x = g_ScalerInternalInt.size() > 0 ? g_ScalerInternalInt[0].Value : 0;
  m_pConstantBuffer.iInternalArg.y = g_ScalerInternalInt.size() > 1 ? g_ScalerInternalInt[1].Value : 0;
  //the dest become the source after the first pass

  Context.SetDynamicConstantBufferView(1, sizeof(CONSTANT_BUFFER_4F_4int), &m_pConstantBuffer);
  Context.Draw(3);


  
  m_bFirstPass = false;
}
*/
CD3D11DynamicScaler::CD3D11DynamicScaler(std::wstring filename,bool *res)
{
  m_pFilename = filename;
  CShaderFileLoader* shader;
  shader = new CShaderFileLoader(filename);
  m_pDesc = {};
  m_pDesc.name = "Bicubic";
  
  shader->Compile(m_pDesc, 0, &m_pOption.parameters);
  
  /*res = (bool*)shader->Compile(m_pDesc, true);
  for (int dd = 0 ; dd < m_pDesc.constants.size(); dd++)
  {
    m_pDesc.constants.at(dd).currentValue = m_pDesc.constants.at(dd).defaultValue;
  }*/
    
  m_pScaler = new CD3DDSShader();
  
}

void CD3D11DynamicScaler::Init()
{
  bool res = m_pScaler->Create(m_pDesc, m_pOption);
}

void CD3D11DynamicScaler::Init(DXGI_FORMAT srcfmt,Com::SmartRect src,Com::SmartRect dst)
{
  m_srcRect = src;

  for (ShaderIntermediateTextureDesc x : m_pDesc.textures)
  {
    Com::SmartRect texsize = src;
    if (x.name != "INPUT")
    {
      if (x.sizeExpr.first == "INPUT_WIDTH")
      {
        texsize.right = src.right;
      }
      if (x.sizeExpr.second == "INPUT_HEIGHT")
      {
        texsize.bottom = src.bottom;
      }
      if (x.sizeExpr.first == "OUTPUT_WIDTH")
      {
        texsize.right = dst.right;
      }
      if (x.sizeExpr.second == "OUTPUT_HEIGHT")
      {
        texsize.bottom = dst.bottom;
      }
      std::wstring currenttex(x.name.begin(), x.name.end());
      
      //m_pScaler->CreateDynTexture(currenttex,src, DXGI_FORMAT_MAPPING[(int)x.format]);
    }
    
  }

  
  /*setting passes here*/
  /*
  for (ShaderPassDesc i : m_pDesc.passes)
  {
    GraphicsPSO pso;
    
    pso.SetRootSignature(D3D12Engine::g_RootScalers);
    pso.SetRasterizerState(D3D12Engine::RasterizerDefault);
    pso.SetBlendState(D3D12Engine::Blendfxrcnnx);
    pso.SetDepthStencilState(D3D12Engine::DepthStateDisabled);
    pso.SetSampleMask(0xFFFFFFFF);
    pso.SetInputLayout(0, nullptr);
    
    pso.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    pso.SetVertexShader(g_pScreenQuadPresentVS, sizeof(g_pScreenQuadPresentVS));
    pso.SetPixelShader(i.cso->GetBufferPointer(), i.cso->GetBufferSize());
    DXGI_FORMAT formats[4];
    for (UINT idx = 0; idx < i.outputs.size(); idx++)
    {
      formats[idx] = m_pScaler->GetDynTexture(i.outputs.at(idx)).GetFormat();
    }
    if (i.outputs.size() == 0)
      pso.SetRenderTargetFormat(srcfmt, DXGI_FORMAT_UNKNOWN);
    else
      pso.SetRenderTargetFormats(i.outputs.size(), formats, DXGI_FORMAT_UNKNOWN);
    
    pso.Finalize();
    m_pPSO.push_back(pso);
  }*/
}

void CD3D11DynamicScaler::Render(Com::SmartRect dstrect, CD3DTexture& dest, CD3DTexture& source)
{
  /*
  Context.SetRootSignature(D3D12Engine::g_RootScalers);
  Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  Context.SetViewportAndScissor(0, 0, source.GetWidth(), source.GetHeight());
  int passes = 0;
  std::vector<DWParam> params;
  DWParam inputx = (float)1.0f / (float)m_srcRect.Width();
  DWParam inputy = (float)1.0f / (float)m_srcRect.Height();
  DWParam outputx = (float)1.0f / (float)dstrect.Width();
  DWParam outputy = (float)1.0f / (float)dstrect.Height();
  DWParam scalex = (float)inputx.Float * (float)dstrect.Width();
  DWParam scaley = (float)inputy.Float * (float)dstrect.Height();
  for (ShaderConstantDesc ss : m_pDesc.constants)
  {
    if (ss.currentValue.index() != 0)
    {
      //sometimes value are in monostate so cant extract need to fix in the shaders loader
      if (ss.type == ShaderParameterDesc::Int)
        params.push_back(std::get<int>(ss.currentValue));
      if (ss.type == ShaderParameterDesc::Float)
        params.push_back(std::get<float>(ss.currentValue));
    }
    else
    {
      if (ss.type == ShaderParameterDesc::Int)
        params.push_back(std::get<int>(ss.defaultValue));
      if (ss.type == ShaderParameterDesc::Float)
        params.push_back(std::get<float>(ss.defaultValue));
    }
  }

  for (ShaderValueConstantDesc ss : m_pDesc.valueConstants)
  {
    //input and output can be float or int so important to cast them as the right type
    if (ss.valueExpr == "INPUT_PT_X")
      params.push_back(inputx);
    else if (ss.valueExpr == "INPUT_PT_Y")
      params.push_back(inputy);
    else if (ss.valueExpr == "INPUT_HEIGHT")
      params.push_back(ss.type == ShaderParameterDesc::Int ? DWParam((int)m_srcRect.Height()) : DWParam((float)m_srcRect.Height()));
    else if (ss.valueExpr == "INPUT_WIDTH")
      params.push_back(ss.type == ShaderParameterDesc::Int ? DWParam((int)m_srcRect.Width()) : DWParam((float)m_srcRect.Width()));
    else if (ss.valueExpr == "OUTPUT_WIDTH")
      params.push_back(ss.type == ShaderParameterDesc::Int ? DWParam((int)dstrect.Width()) : DWParam((float)dstrect.Width()));
    else if (ss.valueExpr == "OUTPUT_HEIGHT")
      params.push_back(ss.type == ShaderParameterDesc::Int ? DWParam((int)dstrect.Height()) : DWParam((float)dstrect.Height()));
    else if (ss.valueExpr == "OUTPUT_PT_X")
      params.push_back(outputx);
    else if (ss.valueExpr == "OUTPUT_PT_Y")
      params.push_back(outputy);
    else if (ss.valueExpr == "SCALE_X")
      params.push_back(scalex);
    else if (ss.valueExpr == "SCALE_Y")
      params.push_back(scaley);
    else
    {
      if (ss.valueExpr == "1/SCALE_X")
        params.push_back((float)1.0 / scalex.Float);
      else if (ss.valueExpr == "1/SCALE_Y")
        params.push_back((float)1.0 / scaley.Float);
      else
        assert(0);
    }

  }
  
  Context.SetDynamicDescriptor(0, 0, source.GetSRV());
  for (ShaderPassDesc i : m_pDesc.passes)
  {
    //set shader resource view
    if (i.inputs.size() != 0)
    {
      if (i.inputs.size()>1)
        m_pScaler->SetDynTextureSrv(Context, i.inputs, 0, source,true);
    }
      
    //set render target
    if (i.outputs.size() != 0)
      m_pScaler->SetDynRenderTargets(Context, i.outputs,true);
    else
    {
      //last pass so set viewport and output render target
      Context.SetRenderTarget(dest.GetRTV());
      //Context.BeginResourceTransition(dest, D3D12_RESOURCE_STATE_RENDER_TARGET);
      Context.SetViewportAndScissor(dstrect.left, dstrect.top, dstrect.Width(), dstrect.Height());
    }
    //set the passes pipeline
    Context.SetPipelineState(m_pPSO[passes]);
   


    //set constant buffers 
    Context.SetConstants(1, params);

    
    Context.Draw(3);
    passes += 1;
  }
  
  */
}

void CD3D11DynamicScaler::Unload()
{
  //if (m_pScaler)
    //m_pScaler->FreeDynTexture();
  m_pScaler = nullptr;
}
CD3D11DynamicScaler::~CD3D11DynamicScaler()
{
  Unload();
}

