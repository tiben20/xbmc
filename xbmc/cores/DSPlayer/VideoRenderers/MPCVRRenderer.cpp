﻿/*
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

using namespace Microsoft::WRL;


std::shared_ptr<CMPCVRRenderer> CMPCVRRenderer::Get()
{
  static std::shared_ptr<CMPCVRRenderer> sVideoRenderer(new CMPCVRRenderer);
  return sVideoRenderer;
}

CMPCVRRenderer::CMPCVRRenderer()
{
}

CMPCVRRenderer::~CMPCVRRenderer()
{
  Release();
}

void CMPCVRRenderer::Release()
{
  
  m_IntermediateTarget.Release();
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
  if (DX::DeviceResources::Get()->GetBackBuffer().GetWidth() == m_IntermediateTarget.GetWidth() && DX::DeviceResources::Get()->GetBackBuffer().GetHeight() == m_IntermediateTarget.GetHeight() )
    DX::DeviceResources::Get()->GetD3DContext()->CopyResource(DX::DeviceResources::Get()->GetBackBuffer().Get(), m_IntermediateTarget.Get());
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
  return true;
}

void CMPCVRRenderer::CheckVideoParameters()
{
}
