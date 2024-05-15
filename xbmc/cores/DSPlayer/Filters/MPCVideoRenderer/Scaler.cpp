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
#include "DSUtil/DSUtil.h"

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

CD3DScaler::CD3DScaler()
{
//add initialisation
}

CD3DScaler::~CD3DScaler()
{
  Release();
}

bool CD3DScaler::Create(const ShaderDesc& desc, const ShaderOption& option)
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
  m_pSamplers.resize(desc.samplers.size());
  for (UINT i = 0; i < m_pSamplers.size(); ++i) {
    const ShaderSamplerDesc& samDesc = desc.samplers[i];
    m_pSamplers[i] = CMPCVRRenderer::Get()->GetSampler(
      samDesc.filterType == ShaderSamplerFilterType::Linear ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT,
      samDesc.addressType == ShaderSamplerAddressType::Clamp ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP
    );

    if (!m_pSamplers[i]) {
      CLog::Log(LOGERROR,"Failed to create sampler{}", samDesc.name);
      return false;
    }
  }

  // 创建中间纹理
  // 第一个为 INPUT，第二个为 OUTPUT
  m_pTextures.resize(desc.textures.size());
  
  m_pTextures[0] = CMPCVRRenderer::Get()->GetIntermediateTarget();

  // 创建输出纹理，格式始终是 DXGI_FORMAT_R8G8B8A8_UNORM
  m_pTextures[1].Create(outputSize.cx, outputSize.cy, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, FORMAT_DESCS[(uint32_t)desc.textures[1].format].dxgiFormat);
    /*= DirectXHelper::CreateTexture2D(
    DX::DeviceResources::GetD3DDevice(),
    FORMAT_DESCS[(uint32_t)desc.textures[1].format].dxgiFormat,
    outputSize.cx,
    outputSize.cy,
    D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS
  );

  *inOutTexture = m_pTextures[1].get();
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
      m_pTextures[i] = TextureLoader::Load(
        StrUtils::UTF8ToUTF16(texPath).c_str(), deviceResources.GetD3DDevice());
      if (!m_pTextures[i]) {
        CLog::Log(LOGERROR,fmt::format("Loading texture {} failed", texDesc.source));
        return false;
      }

      if (texDesc.format != ShaderIntermediateTextureFormat::UNKNOWN) {
        // Check if texture format matches
        D3D11_TEXTURE2D_DESC srcDesc{};
        m_pTextures[i]->GetDesc(&srcDesc);
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

      if (!m_pTextures[i].Create(texSize.cx, texSize.cy, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, FORMAT_DESCS[(UINT)texDesc.format].dxgiFormat))
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
      if (!m_pTextures[i]) {
        CLog::Log(LOGERROR,"创建纹理失败");
        return false;
      }*/
    }
  }

  m_pComputeShaders.resize(desc.passes.size());
  m_pSRVs.resize(desc.passes.size());
  m_pUAVs.resize(desc.passes.size());
  for (UINT i = 0; i < m_pComputeShaders.size(); ++i) {
    const ShaderPassDesc& passDesc = desc.passes[i];
#if 1
    HRESULT hr = DX::DeviceResources::Get()->GetD3DDevice()->CreateComputeShader(
      passDesc.cso->GetBufferPointer(), passDesc.cso->GetBufferSize(), nullptr, m_pComputeShaders[i].ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
      CLog::Log(LOGERROR, "Failed to create compute shader");
      return false;
    }

    m_pSRVs[i].resize(passDesc.inputs.size());
    for (UINT j = 0; j < passDesc.inputs.size(); ++j) {
      auto srv = m_pSRVs[i][j] = CMPCVRRenderer::Get()->GetShaderResourceView(m_pTextures[passDesc.inputs[j]].Get());
      if (!srv) {
        CLog::Log(LOGERROR, "GetShaderResourceView failed");
        return false;
      }
    }

    m_pUAVs[i].resize(passDesc.outputs.size() * 2);
    for (UINT j = 0; j < passDesc.outputs.size(); ++j) {
      auto uav = m_pUAVs[i][j] = CMPCVRRenderer::Get()->GetUnorderedAccessView(m_pTextures[passDesc.outputs[j]].Get());
      if (!uav) {
        CLog::Log(LOGERROR, "GetUnorderedAccessView failed");
        return false;
      }
    }
#else
    HRESULT hr = deviceResources.GetD3DDevice()->CreateComputeShader(
      passDesc.cso->GetBufferPointer(), passDesc.cso->GetBufferSize(), nullptr, m_pComputeShaders[i].put());
    if (FAILED(hr)) {
      Logger::Get().ComError("创建计算着色器失败", hr);
      return false;
    }

    m_pSRVs[i].resize(passDesc.inputs.size());
    for (UINT j = 0; j < passDesc.inputs.size(); ++j) {
      auto srv = m_pSRVs[i][j] = descriptorStore.GetShaderResourceView(m_pTextures[passDesc.inputs[j]].get());
      if (!srv) {
        CLog::Log(LOGERROR,"GetShaderResourceView 失败");
        return false;
      }
    }

    m_pUAVs[i].resize(passDesc.outputs.size() * 2);
    for (UINT j = 0; j < passDesc.outputs.size(); ++j) {
      auto uav = m_pUAVs[i][j] = descriptorStore.GetUnorderedAccessView(m_pTextures[passDesc.outputs[j]].get());
      if (!uav) {
        CLog::Log(LOGERROR,"GetUnorderedAccessView 失败");
        return false;
      }
    }
#endif
    D3D11_TEXTURE2D_DESC outputDesc;
    m_pTextures[passDesc.outputs[0]].GetDesc(&outputDesc);
    m_pDispatches.emplace_back(
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


void CD3DScaler::Release()
{
  Unregister();
  OnDestroyDevice(false);
}

void CD3DScaler::OnDestroyDevice(bool fatal)
{
//todo add destroy on surface and uavs and others
}

void CD3DScaler::OnCreateDevice()
{
  
}

SIZE CD3DScaler::CalcOutputSize(const std::pair<std::string, std::string>& outputSizeExpr, const ShaderOption& option, SIZE scalingWndSize, SIZE inputSize, mu::Parser& exprParser)
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

bool CD3DScaler::InitializeConstants(const ShaderDesc& desc, const ShaderOption& option, SIZE inputSize, SIZE outputSize)
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
  m_pConstants.resize((builtinConstantCount + psStylePassParams + (isInlineParams ? 0 : desc.params.size()) + 3) / 4 * 4);
  // cbuffer __CB1 : register(b0) {
  //     uint2 __inputSize;
  //     uint2 __outputSize;
  //     float2 __inputPt;
  //     float2 __outputPt;
  //     float2 __scale;
  //     [PARAMETERS...]
  // );
  m_pConstants[0].uintVal = inputSize.cx;
  m_pConstants[1].uintVal = inputSize.cy;
  m_pConstants[2].uintVal = outputSize.cx;
  m_pConstants[3].uintVal = outputSize.cy;
  m_pConstants[4].floatVal = 1.0f / inputSize.cx;
  m_pConstants[5].floatVal = 1.0f / inputSize.cy;
  m_pConstants[6].floatVal = 1.0f / outputSize.cx;
  m_pConstants[7].floatVal = 1.0f / outputSize.cy;
  m_pConstants[8].floatVal = outputSize.cx / (FLOAT)inputSize.cx;
  m_pConstants[9].floatVal = outputSize.cy / (FLOAT)inputSize.cy;

  // Parameters required for PS style channels
  Constant32* pCurParam = m_pConstants.data() + builtinConstantCount;
  if (psStylePassParams > 0) {
    for (UINT i = 0, end = (UINT)desc.passes.size() - 1; i < end; ++i) {
      if (desc.passes[i].isPSStyle) {
        D3D11_TEXTURE2D_DESC outputDesc;
        m_pTextures[desc.passes[i].outputs[0]].GetDesc(&outputDesc);
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
  bd.ByteWidth = 4 * (UINT)m_pConstants.size();
  bd.Usage = D3D11_USAGE_DEFAULT;
  bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  

  D3D11_SUBRESOURCE_DATA initData{};
  initData.pSysMem = m_pConstants.data();

  
  HRESULT hr = DX::DeviceResources::Get()->GetD3DDevice()->CreateBuffer(&bd, &initData, m_pConstantBuffer.ReleaseAndGetAddressOf());
  if (FAILED(hr))
  {
    CLog::Log(LOGERROR, "CreateBuffer failed");
    return false;
  }

  return true;
}

void CD3DScaler::Draw(CMPCVRRenderer* renderer)
{
  ID3D11Buffer* t = m_pConstantBuffer.Get();
  DX::DeviceResources::Get()->GetD3DContext()->CSSetConstantBuffers(0, 1, &t);
  DX::DeviceResources::Get()->GetD3DContext()->CSSetSamplers(0, (UINT)m_pSamplers.size(), m_pSamplers.data());
    for (uint32_t i = 0; i < m_pDispatches.size(); ++i)
    {
      DX::DeviceResources::Get()->GetD3DContext()->CSSetShader(m_pComputeShaders[i].Get(), nullptr, 0);

      DX::DeviceResources::Get()->GetD3DContext()->CSSetShaderResources(0, (UINT)m_pSRVs[i].size(), m_pSRVs[i].data());
      UINT uavCount = (UINT)m_pUAVs[i].size() / 2;
      DX::DeviceResources::Get()->GetD3DContext()->CSSetUnorderedAccessViews(0, uavCount, m_pUAVs[i].data(), nullptr);

      DX::DeviceResources::Get()->GetD3DContext()->Dispatch(m_pDispatches[i].first, m_pDispatches[i].second, 1);

      DX::DeviceResources::Get()->GetD3DContext()->CSSetUnorderedAccessViews(0, uavCount, m_pUAVs[i].data() + uavCount, nullptr);
      renderer->OnEndPass();
    }

}

CD3D11DynamicScaler::CD3D11DynamicScaler(std::wstring filename,bool *res)
{
  m_pFilename = filename;
  CShaderFileLoader* shader;
  shader = new CShaderFileLoader(filename);
  m_pDesc = {};
  std::string scalername;
  scalername = WToA(filename);
  StringUtils::Replace(scalername, ".hlsl", "");
  size_t lastpos = scalername.find_last_of("/");
  if (lastpos != std::string::npos)
    scalername.erase(0, lastpos+1);

  m_pDesc.name = scalername;

  shader->Compile(m_pDesc, 0, &m_pOption.parameters);
  
  /*res = (bool*)shader->Compile(m_pDesc, true);
  for (int dd = 0 ; dd < m_pDesc.constants.size(); dd++)
  {
    m_pDesc.constants.at(dd).currentValue = m_pDesc.constants.at(dd).defaultValue;
  }*/
    
  m_pScaler = new CD3DScaler();
  
}

void CD3D11DynamicScaler::Init()
{
  bool res = m_pScaler->Create(m_pDesc, m_pOption);
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

