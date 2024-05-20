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
#include <Filters/MPCVideoRenderer/DSResource.h>
#include "guilib/GUIShaderDX.h"

#define GetDevice DX::DeviceResources::Get()->GetD3DDevice()

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

  CD3D11DynamicScaler* cur = new CD3D11DynamicScaler(L"special://xbmc/system/shaders/mpcvr/Bicubic.hlsl", &res);
  
  m_pShaders.push_back(cur);
  
}

void CMPCVRRenderer::InitShaders()
{
  CD3DDSPixelShader shdr;
  LPVOID data;
  DWORD size;
  D3D11_INPUT_ELEMENT_DESC Layout[] = {
  {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
  {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
  };

  if (!shdr.LoadFromFile(IDF_VS_11_SIMPLE))
    CLog::Log(LOGERROR, "{} failed loading {}", __FUNCTION__, IDF_VS_11_SIMPLE);

  data = shdr.GetData();
  size = shdr.GetSize();

  GetDevice->CreateVertexShader(data, size, nullptr, &m_pVS_Simple);
  GetDevice->CreateInputLayout(Layout, std::size(Layout), data, size, &m_pVSimpleInputLayout);

  if (!shdr.LoadFromFile(IDF_PS_11_SIMPLE))
    CLog::Log(LOGERROR, "{} failed loading {}", __FUNCTION__, IDF_VS_11_SIMPLE);

  data = shdr.GetData();
  size = shdr.GetSize();

  GetDevice->CreatePixelShader(data,size,nullptr,&m_pPS_Simple);


  for (int idx = 0; idx < m_pShaders.size(); idx++)
  {
    m_pShaders[idx]->Init();
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

HRESULT CMPCVRRenderer::FillVertexBuffer(const UINT srcW, const UINT srcH, const CRect& srcRect, const int iRotation, const bool bFlip)
{
  DS_VERTEX Vertices[4];
  FillVertices(Vertices, srcW, srcH, srcRect, iRotation, bFlip);

  D3D11_MAPPED_SUBRESOURCE mr;
  HRESULT hr = DX::DeviceResources::Get()->GetD3DContext()->Map(m_pVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mr);
  if (FAILED(hr)) {
    CLog::LogF(LOGINFO, "FillVertexBuffer() : Map() failed");
    return hr;
  }

  memcpy(mr.pData, &Vertices, sizeof(Vertices));
  DX::DeviceResources::Get()->GetD3DContext()->Unmap(m_pVertexBuffer.Get(), 0);

  return hr;
}

void CMPCVRRenderer::FillVertices(DS_VERTEX(&Vertices)[4], const UINT srcW, const UINT srcH, const CRect& srcRect, const int iRotation, const bool bFlip)
{
  const float src_dx = 1.0f / srcW;
  const float src_dy = 1.0f / srcH;
  float src_l = src_dx * srcRect.x1;
  float src_r = src_dx * srcRect.x2;
  const float src_t = src_dy * srcRect.y1;
  const float src_b = src_dy * srcRect.y2;

  POINT points[4];
  switch (iRotation) {
  case 90:
    points[0] = { -1, +1 };
    points[1] = { +1, +1 };
    points[2] = { -1, -1 };
    points[3] = { +1, -1 };
    break;
  case 180:
    points[0] = { +1, +1 };
    points[1] = { +1, -1 };
    points[2] = { -1, +1 };
    points[3] = { -1, -1 };
    break;
  case 270:
    points[0] = { +1, -1 };
    points[1] = { -1, -1 };
    points[2] = { +1, +1 };
    points[3] = { -1, +1 };
    break;
  default:
    points[0] = { -1, -1 };
    points[1] = { -1, +1 };
    points[2] = { +1, -1 };
    points[3] = { +1, +1 };
  }

  if (bFlip) {
    std::swap(src_l, src_r);
  }

  // Vertices for drawing whole texture
  // 2 ___4
  //  |\ |
  // 1|_\|3
  Vertices[0] = { {(float)points[0].x, (float)points[0].y, 0}, {src_l, src_b} };
  Vertices[1] = { {(float)points[1].x, (float)points[1].y, 0}, {src_l, src_t} };
  Vertices[2] = { {(float)points[2].x, (float)points[2].y, 0}, {src_r, src_b} };
  Vertices[3] = { {(float)points[3].x, (float)points[3].y, 0}, {src_r, src_t} };
}

HRESULT CMPCVRRenderer::CreateVertexBuffer()
{
  if (m_pVertexBuffer.Get())
    return S_OK;
  DS_VERTEX Vertices[4];

  CD3D11_BUFFER_DESC bufferDesc(sizeof(Vertices), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

  HRESULT hr = DX::DeviceResources::Get()->GetD3DDevice()->CreateBuffer(&bufferDesc, NULL, m_pVertexBuffer.ReleaseAndGetAddressOf());

  return hr;
}

void CMPCVRRenderer::CopyToBackBuffer()
{
  D3D11_VIEWPORT oldVP;
  UINT oldIVP = 1;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> oldRT;
  ID3D11DeviceContext* pContext = DX::DeviceResources::Get()->GetD3DContext();
  pContext->OMGetRenderTargets(1, &oldRT, nullptr);
  pContext->RSGetViewports(&oldIVP, &oldVP);

  const UINT Stride = sizeof(DS_VERTEX);
  const UINT Offset = 0;

  D3D11_VIEWPORT VP;
  VP.TopLeftX = (FLOAT)m_destRect.x1;
  VP.TopLeftY = (FLOAT)m_destRect.y1;
  VP.Width = (FLOAT)m_destRect.Width();
  VP.Height = (FLOAT)m_destRect.Height();
  VP.MinDepth = 0.0f;
  VP.MaxDepth = 1.0f;
  CRect sourcerect;
  sourcerect.x1 = 0;
  sourcerect.y1 = 0;
  sourcerect.x2 = m_sourceWidth;
  sourcerect.y2 = m_sourceHeight;
  FillVertexBuffer(m_sourceWidth, m_sourceHeight, sourcerect/*m_sourceRect*/, 0, 0);

  ID3D11SamplerState* pSampler = GetSampler(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
  // Set resources

  
  pContext->OMSetRenderTargets(1, DX::DeviceResources::Get()->GetBackBuffer().GetAddressOfRTV(), nullptr);
  pContext->RSSetViewports(1, &VP);
  //pContext->OMSetBlendState(nullptr, nullptr, D3D11_DEFAULT_SAMPLE_MASK);
  pContext->VSSetShader(m_pVS_Simple.Get(), nullptr, 0);
  pContext->PSSetShader(m_pPS_Simple.Get(), nullptr, 0);

  pContext->IASetInputLayout(m_pVSimpleInputLayout.Get());

  pContext->PSSetShaderResources(0, 1, m_pShaders[0]->GetOutputSurface().GetAddressOfSRV());
  //pContext->PSSetSamplers(0, 1, &pSampler);
  //pContext->PSSetConstantBuffers(0, 1, &pConstantBuffer);
  pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  pContext->IASetVertexBuffers(0, 1, m_pVertexBuffer.GetAddressOf(), &Stride, &Offset);

  // Draw textured quad onto render target
  pContext->Draw(4, 0);
  CRenderSystemDX* renderingDX = dynamic_cast<CRenderSystemDX*>(CServiceBroker::GetRenderSystem());
  renderingDX->GetGUIShader()->ApplyStateBlock();

  ID3D11ShaderResourceView* views[4] = {};
  pContext->PSSetShaderResources(0, 4, views);

  pContext->OMSetRenderTargets(1, oldRT.GetAddressOf(), nullptr);
  pContext->RSSetViewports(1, &oldVP);
  //pContext->IASetInputLayout(nullptr);
}

void CMPCVRRenderer::Reset()
{
  m_pDisjointQuery = nullptr;
  m_pStartQuery = nullptr;
  m_pPassQueries.clear();
  m_IntermediateTarget.Release();
  
  
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

bool CMPCVRRenderer::CreateInputTarget(unsigned int width, unsigned int height, DXGI_FORMAT format)
{
  // don't create new one if it exists with requested size and format
  if (m_InputTarget.Get() && m_InputTarget.GetFormat() == format &&
    m_InputTarget.GetWidth() == width && m_InputTarget.GetHeight() == height)
    return true;

  if (m_InputTarget.Get())
    m_InputTarget.Release();

  CLog::LogF(LOGDEBUG, "{} creating input target {}x{} format {}.", __FUNCTION__, width, height,
    DX::DXGIFormatToString(format));

  if (!m_InputTarget.Create(width, height, 1,
    D3D11_USAGE_DEFAULT, format, nullptr, 0U, "CMPCVRRenderer input Target"))
  {
    CLog::LogF(LOGERROR, "input target creation failed.");
    return false;
  }

  InitShaders();

  Start(m_pShaders.at(0)->GetNumberPasses());

  return true;
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
  if (!m_InputTarget.Get())
    return;
  //m_sourceRect source of the video
  //m_destRect destination rectangle
  //GetScreenRect screen rectangle
  //destRect destination
  //m_sourceWidth
  //m_sourceHeight
  if (!(m_destRect.Width() == m_IntermediateTarget.GetWidth() && m_destRect.Height() == m_IntermediateTarget.GetHeight()))
    CreateIntermediateTarget(m_destRect.Width(), m_destRect.Height(), false, DX::DeviceResources::Get()->GetBackBuffer().GetFormat());
    //CreateIntermediateTarget(DX::DeviceResources::Get()->GetBackBuffer().GetWidth(), DX::DeviceResources::Get()->GetBackBuffer().GetHeight(), false, DX::DeviceResources::Get()->GetBackBuffer().GetFormat());


  OnBeginEffects();
  for (int idx = 0; idx < m_pShaders.size(); idx++)
  {
    m_pShaders.at(idx)->Draw(this);
  }
  OnEndEffects();
  
  D3D11_BOX srcBox = {};
  srcBox.left = m_destRect.x1;
  srcBox.top = m_destRect.y1;
  srcBox.front = 0;
  srcBox.right = m_destRect.x2;
  srcBox.bottom = m_destRect.y2;
  srcBox.back = 1;
  
  //Copy subresource as the problem to not be able to have negative left and top so its bugging on scaling
  //DX::DeviceResources::Get()->GetD3DContext()->CopySubresourceRegion(DX::DeviceResources::Get()->GetBackBuffer().Get(), 0, 
  //                                                                   m_destRect.x1 ,m_destRect.y1,0, m_pShaders[0]->GetOutputSurface().Get(),0, &srcBox);
  CopyToBackBuffer();
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
  CreateVertexBuffer();
  m_sourceRect.x1 = 0;
  m_sourceRect.x2 = width;
  m_sourceRect.y1 = 0;
  m_sourceRect.y2 = height;
  m_fps = fps;
  CalculateFrameAspectRatio(width, height);
  SetViewMode(m_videoSettings.m_ViewMode);
  //CreateInputTarget(m_sourceWidth, m_sourceHeight);
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

  if (!m_IntermediateTarget.Create(width,height, 1,
                                   dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT, format,nullptr,0U,"CMPCVRRenderer Intermediate Target"))
  {
    CLog::LogF(LOGERROR, "intermediate target creation failed.");
    return false;
  }
  //only reset if 
  if (m_InputTarget.Get())
  m_pShaders[0]->ResetOutputTexture(width, height, format);
  
  return true;
}

void CMPCVRRenderer::CheckVideoParameters()
{
}


