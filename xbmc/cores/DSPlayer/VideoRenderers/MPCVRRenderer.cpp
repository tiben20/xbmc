/*
 *  Copyright (C) 2017-2019 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "MPCVRRenderer.h"


#include "ServiceBroker.h"
#include "rendering/dx/DirectXHelper.h"
#include "rendering/dx/RenderContext.h"
#include "utils/MemUtils.h"
#include "utils/log.h"
#include "windowing/GraphicContext.h"
#include "d3d11_1.h"

using namespace Microsoft::WRL;


std::shared_ptr<CMPCVRRenderer> CMPCVRRenderer::Get()
{
  static std::shared_ptr<CMPCVRRenderer> sVideoRenderer(new CMPCVRRenderer);
  return sVideoRenderer;
}

CMPCVRRenderer::CMPCVRRenderer()
{
  LoadShaders();
}

CMPCVRRenderer::~CMPCVRRenderer()
{
  Release();
}

void CMPCVRRenderer::Release()
{
  Stop();
  m_IntermediateTarget.Release();
}

void CMPCVRRenderer::LoadShaders()
{
  bool res;

  CD3D11DynamicScaler* cur = new CD3D11DynamicScaler(L"special://xbmc/system/shaders/mpcvr/Anime4K_Restore_L.hlsl", &res);
  
  m_pShaders.push_back(cur);
  
}

void CMPCVRRenderer::InitShaders()
{
  for (int idx = 0; idx < m_pShaders.size(); idx++)
  {
    m_pShaders.at(idx)->Init();
  }
  
}

void CMPCVRRenderer::Start(uint32_t passcount)
{
  assert(m_pPassQueries.empty());
  m_pPassQueries.resize(passcount);

  D3D11_QUERY_DESC desc{};
  desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
  HRESULT hr = DX::DeviceResources::Get()->GetD3DDevice()->CreateQuery(&desc, m_pDisjointQuery.ReleaseAndGetAddressOf());

  desc.Query = D3D11_QUERY_TIMESTAMP;
  hr = DX::DeviceResources::Get()->GetD3DDevice()->CreateQuery(&desc, m_pStartQuery.ReleaseAndGetAddressOf());
  for (Microsoft::WRL::ComPtr<ID3D11Query>& query : m_pPassQueries)
    hr = DX::DeviceResources::Get()->GetD3DDevice()->CreateQuery(&desc, query.ReleaseAndGetAddressOf());
  
}

void CMPCVRRenderer::Stop()
{
  m_pDisjointQuery = nullptr;
  m_pStartQuery = nullptr;
  m_pPassQueries.clear();
}

void CMPCVRRenderer::OnBeginEffects()
{
  if (m_pPassQueries.empty())
    return;
  DX::DeviceResources::Get()->GetD3DContext()->Begin(m_pDisjointQuery.Get());
  DX::DeviceResources::Get()->GetD3DContext()->End(m_pStartQuery.Get());
  m_pCurrentPasses = 0;
}

void CMPCVRRenderer::OnEndEffects()
{
  if (m_pPassQueries.empty())
    return;

  DX::DeviceResources::Get()->GetD3DContext()->End(m_pDisjointQuery.Get());
}

void CMPCVRRenderer::OnEndPass()
{
  if (m_pPassQueries.empty())
    return;

  DX::DeviceResources::Get()->GetD3DContext()->End(m_pPassQueries[m_pCurrentPasses++].Get());
}

ID3D11SamplerState* CMPCVRRenderer::GetSampler(D3D11_FILTER filterMode, D3D11_TEXTURE_ADDRESS_MODE addressMode)
{
  auto key = std::make_pair(filterMode, addressMode);
  auto it = m_pSamplers.find(key);
  if (it != m_pSamplers.end()) {
    return it->second.Get();
  }

  Microsoft::WRL::ComPtr<ID3D11SamplerState> sam;

  D3D11_SAMPLER_DESC desc{};
  desc.Filter = filterMode;
    desc.AddressU = addressMode;
    desc.AddressV = addressMode;
    desc.AddressW = addressMode ;
    desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
  HRESULT hr = DX::DeviceResources::Get()->GetD3DDevice()->CreateSamplerState(&desc, sam.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    
    return nullptr;
  }

  return m_pSamplers.emplace(key, std::move(sam)).first->second.Get();
}

ID3D11ShaderResourceView* CMPCVRRenderer::GetShaderResourceView(ID3D11Texture2D* texture)
{
  if (auto it = m_pShaderRSV.find(texture); it != m_pShaderRSV.end()) {
    return it->second.Get();
  }

  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
  HRESULT hr = DX::DeviceResources::Get()->GetD3DDevice()->CreateShaderResourceView(texture, nullptr, srv.ReleaseAndGetAddressOf());
  if (FAILED(hr))
  {
    CLog::Log(LOGERROR,"CreateShaderResourceView failed", hr);
    return nullptr;
  }

  return m_pShaderRSV.emplace(texture, std::move(srv)).first->second.Get();
}

ID3D11UnorderedAccessView* CMPCVRRenderer::GetUnorderedAccessView(ID3D11Texture2D* texture)
{
  if (auto it = m_pUAVViews.find(texture); it != m_pUAVViews.end()) {
    return it->second.Get();
  }

  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;

  D3D11_UNORDERED_ACCESS_VIEW_DESC desc{};
  desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;

  HRESULT hr = DX::DeviceResources::Get()->GetD3DDevice()->CreateUnorderedAccessView(texture, &desc, uav.ReleaseAndGetAddressOf());
  if (FAILED(hr))
  {
    CLog::Log(LOGERROR, "CreateUnorderedAccessView failed", hr);
    return nullptr;
  }

  return m_pUAVViews.emplace(texture, std::move(uav)).first->second.Get();
}

ID3D11UnorderedAccessView* CMPCVRRenderer::GetUnorderedAccessView(ID3D11Buffer* buffer, uint32_t numElements, DXGI_FORMAT format)
{
  if (auto it = m_pUAVViews.find(buffer); it != m_pUAVViews.end()) {
    return it->second.Get();
  }

  Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;

  D3D11_UNORDERED_ACCESS_VIEW_DESC desc{};
  desc.Format = format;
  desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
  desc.Buffer = {};
  desc.Buffer.NumElements = numElements;
    
  

  HRESULT hr = DX::DeviceResources::Get()->GetD3DDevice()->CreateUnorderedAccessView(buffer, &desc, uav.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    CLog::Log(LOGERROR, "CreateUnorderedAccessView failed", hr);
    return nullptr;
  }

  return m_pUAVViews.emplace(buffer, std::move(uav)).first->second.Get();
}

void CMPCVRRenderer::Render(int index,
                           int index2,
                           CD3DTexture& target,
                           const CRect& sourceRect,
                           const CRect& destRect,
                           const CRect& viewRect,
                           unsigned flags)
{
  m_iBufferIndex = index;

  CreateIntermediateTarget(m_viewWidth, m_viewHeight, false);

  Render(target, sourceRect, destRect, viewRect, flags);
}

void CMPCVRRenderer::Render(CD3DTexture& target, const CRect& sourceRect, const CRect& destRect, const CRect& viewRect, unsigned flags)
{
  if (m_iNumBuffers == 0)
    return;
  if (m_viewWidth != static_cast<unsigned>(viewRect.Width()) ||
    m_viewHeight != static_cast<unsigned>(viewRect.Height()))
  {
    m_viewWidth = static_cast<unsigned>(viewRect.Width());
    m_viewHeight = static_cast<unsigned>(viewRect.Height());
    
  }
  

  // Restore our view port.
  DX::Windowing()->RestoreViewPort();
  DX::Windowing()->ApplyStateBlock();
}

bool CMPCVRRenderer::Configure(const VideoPicture& picture, float fps, unsigned int orientation)
{
  m_bConfigured = true;

  return true;
}

bool CMPCVRRenderer::Flush(bool saveBuffers)
{
  CLog::Log(LOGDEBUG, "{}", __FUNCTION__);
  return false;
}

void CMPCVRRenderer::RenderUpdate(int index, int index2, bool clear, unsigned int flags, unsigned int alpha)
{

  if (clear)
    CServiceBroker::GetWinSystem()->GetGfxContext().Clear(DX::Windowing()->UseLimitedColor() ? 0x101010 : 0);
  DX::Windowing()->SetAlphaBlendEnable(alpha < 255);

  ManageRenderArea();
  //m_sourceRect source of the video
  //m_destRect destination rectangle
  //GetScreenRect screen rectangle
  OnBeginEffects();
  for (int idx = 0; idx < m_pShaders.size(); idx++)
  {
    m_pShaders.at(idx)->Draw(this);
  }
  OnEndEffects();
  
  if (DX::DeviceResources::Get()->GetBackBuffer().GetWidth() == m_IntermediateTarget.GetWidth() && DX::DeviceResources::Get()->GetBackBuffer().GetHeight() == m_IntermediateTarget.GetHeight() )
    DX::DeviceResources::Get()->GetD3DContext()->CopyResource(DX::DeviceResources::Get()->GetBackBuffer().Get(), m_pShaders[0]->GetOutputSurface().Get());
  else
  {
    CreateIntermediateTarget(DX::DeviceResources::Get()->GetBackBuffer().GetWidth(), DX::DeviceResources::Get()->GetBackBuffer().GetHeight(), false, DX::DeviceResources::Get()->GetBackBuffer().GetFormat());
  }

  DX::Windowing()->SetAlphaBlendEnable(true);
  
}

bool CMPCVRRenderer::Configure(unsigned int width, unsigned int height, unsigned int d_width, unsigned int d_height, float fps)
{
  if (m_sourceWidth != width
    || m_sourceHeight != height)
  {
    m_sourceWidth = width;
    m_sourceHeight = height;
    // need to recreate textures
  }
  m_sourceRect.x1 = 0;
  m_sourceRect.x2 = width;
  m_sourceRect.y1 = 0;
  m_sourceRect.y2 = height;
  m_fps = fps;
  CalculateFrameAspectRatio(width, height);
  SetViewMode(m_videoSettings.m_ViewMode);
  CreateIntermediateTarget(width, height, false);

  return true;
}

CD3DTexture& CMPCVRRenderer::GetIntermediateTarget()
{
  if (m_IntermediateTarget.Get() == nullptr)
  {
  }
  return m_IntermediateTarget;
}

CRect CMPCVRRenderer::GetScreenRect() const
{
  CRect screenRect(0.f, 0.f,
    static_cast<float>(CServiceBroker::GetWinSystem()->GetGfxContext().GetWidth()),
    static_cast<float>(CServiceBroker::GetWinSystem()->GetGfxContext().GetHeight()));

  switch (CServiceBroker::GetWinSystem()->GetGfxContext().GetStereoMode())
  {
  case RENDER_STEREO_MODE_SPLIT_HORIZONTAL:
    screenRect.y2 *= 2;
    break;
  case RENDER_STEREO_MODE_SPLIT_VERTICAL:
    screenRect.x2 *= 2;
    break;
  default:
    break;
  }

  return screenRect;
}

void CMPCVRRenderer::GetVideoRect(CRect& source, CRect& dest, CRect& view) const
{
  source = m_sourceRect;
  dest = m_destRect;
  view = m_viewRect;
}

float CMPCVRRenderer::GetAspectRatio() const
{
  CLog::Log(LOGDEBUG, "{}", __FUNCTION__);
  return 0.0f;
}

void CMPCVRRenderer::SettingOptionsRenderMethodsFiller(const std::shared_ptr<const CSetting>& setting, std::vector<IntegerSettingOption>& list, int& current, void* data)
{
  CLog::Log(LOGDEBUG, "{}", __FUNCTION__);
}

void CMPCVRRenderer::SetVideoSettings(const CVideoSettings& settings)
{
  CLog::Log(LOGDEBUG, "{}", __FUNCTION__);
}



bool CMPCVRRenderer::CreateIntermediateTarget(unsigned width,
                                             unsigned height,
                                             bool dynamic,
                                             DXGI_FORMAT format)
{
  // No format specified by renderer
  if (format == DXGI_FORMAT_UNKNOWN)
    format = DX::Windowing()->GetBackBuffer().GetFormat();

  // don't create new one if it exists with requested size and format
  if (m_IntermediateTarget.Get() && m_IntermediateTarget.GetFormat() == format &&
      m_IntermediateTarget.GetWidth() == width && m_IntermediateTarget.GetHeight() == height)
    return true;

  if (m_IntermediateTarget.Get())
    m_IntermediateTarget.Release();

  CLog::LogF(LOGDEBUG, "{} creating intermediate target {}x{} format {}.", __FUNCTION__, width, height,
             DX::DXGIFormatToString(format));

  if (!m_IntermediateTarget.Create(DX::Windowing()->GetBackBuffer().GetWidth(), DX::Windowing()->GetBackBuffer().GetHeight(), 1,
                                   dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT, format,nullptr,0U,"CMPCVRRenderer Intermediate Target"))
  {
    CLog::LogF(LOGERROR, "intermediate target creation failed.");
    return false;
  }
  Com::SmartRect srcRect;
  srcRect.top = 0; srcRect.left = 0;
  srcRect.right = m_IntermediateTarget.GetWidth();
  srcRect.bottom = m_IntermediateTarget.GetHeight();
  InitShaders();
  
  Start(m_pShaders.at(0)->GetNumberPasses());
  //m_pShaders.at(0)->Init(DXGI_FORMAT_R8G8B8A8_UNORM, srcRect, srcRect);
  return true;
}

void CMPCVRRenderer::CheckVideoParameters()
{
}

