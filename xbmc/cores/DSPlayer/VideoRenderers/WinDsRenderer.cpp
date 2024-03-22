/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */
 
#if HAS_DS_PLAYER

#include "WinDsRenderer.h"
#include "Util.h"
#include "settings/Settings.h"
#include "guilib/Texture.h"
//#include "windowing/windows/WinSystemWin32DX.h"
#include "settings/AdvancedSettings.h"
#include "threads/SingleLock.h"
#include "utils/log.h"
#include "FileSystem/File.h"
#include "utils/MathUtils.h"
#include "StreamsManager.h"
#include "Filters\DX9AllocatorPresenter.h"
#include "settings/DisplaySettings.h"
#include "settings/MediaSettings.h"
#include "windowing/GraphicContext.h"
#include "application/Application.h"
#include "cores/dsplayer/IDSPlayer.h"

CWinDsRenderer::CWinDsRenderer(): 
  m_bConfigured(false)
  , m_oldVideoRect(0, 0, 0, 0)
{
}

CWinDsRenderer::~CWinDsRenderer()
{
  UnInit();
}

void CWinDsRenderer::SetupScreenshot()
{
  // When taking a screenshot, the CDX9AllocatorPreenter::Paint() method is called, but never CDX9AllocatorPresenter::OnAfterPresent().
  // The D3D device is always locked. Setting bPaintAll to false fixes that.
  CDX9AllocatorPresenter::bPaintAll = false;
}

bool CWinDsRenderer::Configure(unsigned int width, unsigned int height, unsigned int d_width, unsigned int d_height, float fps, unsigned flags, AVPixelFormat format, unsigned extended_format, unsigned int orientation)
{
  if (m_sourceWidth != width
    || m_sourceHeight != height)
  {
    m_sourceWidth = width;
    m_sourceHeight = height;
    // need to recreate textures
  }

  m_fps = fps;
  m_iFlags = flags;
  m_flags = flags;
  m_format = format;

  // calculate the input frame aspect ratio
  CalculateFrameAspectRatio(d_width, d_height);

  SetViewMode(CMediaSettings::GetInstance().GetCurrentVideoSettings().m_ViewMode);
  ManageRenderArea();

  m_bConfigured = true;

  return true;
}

void CWinDsRenderer::Reset()
{
  // todo
}

void CWinDsRenderer::Update()
{
  if (!m_bConfigured) 
    return;

  ManageRenderArea();
}

bool CWinDsRenderer::RenderCapture(CRenderCapture* capture)
{
  if (!m_bConfigured)
    return false;

  bool succeeded = false;

  ID3D11DeviceContext* pContext = g_Windowing.Get3D11Context();

  CRect saveSize = m_destRect;
  saveRotatedCoords();//backup current m_rotatedDestCoords

  m_destRect.SetRect(0, 0, (float)capture->GetWidth(), (float)capture->GetHeight());
  syncDestRectToRotatedPoints();//syncs the changed destRect to m_rotatedDestCoords

  ID3D11DepthStencilView* oldDepthView;
  ID3D11RenderTargetView* oldSurface;
  pContext->OMGetRenderTargets(1, &oldSurface, &oldDepthView);

  capture->BeginRender();
  if (capture->GetState() != CAPTURESTATE_FAILED)
  {
    Render(0);
    capture->EndRender();
    succeeded = true;
  }

  pContext->OMSetRenderTargets(1, &oldSurface, oldDepthView);
  oldSurface->Release();
  SAFE_RELEASE(oldDepthView); // it can be nullptr

  m_destRect = saveSize;
  restoreRotatedCoords();//restores the previous state of the rotated dest coords

  return succeeded;
}

void CWinDsRenderer::RenderUpdate(bool clear, unsigned int flags, unsigned int alpha)
{
  if (clear)
    g_graphicsContext.Clear(m_clearColour);

  if (alpha < 255)
    g_Windowing.SetAlphaBlendEnable(true);
  else
    g_Windowing.SetAlphaBlendEnable(false);

  if (!m_bConfigured)
    return;

  CSingleExit lock(g_graphicsContext);

  ManageRenderArea();

  Render(flags);
}

void CWinDsRenderer::Flush()
{
  PreInit();
  SetViewMode(CMediaSettings::GetInstance().GetCurrentVideoSettings().m_ViewMode);
  ManageRenderArea();

  m_bConfigured = true;
}

void CWinDsRenderer::PreInit()
{
  CSingleExit lock(g_graphicsContext);
  m_bConfigured = false;
  UnInit();

  // setup the background colour
  m_clearColour = g_Windowing.UseLimitedColor() ? (16 * 0x010101) : 0;
  return;
}

void CWinDsRenderer::UnInit()
{
  m_bConfigured = false;
}

void CWinDsRenderer::Render(DWORD flags)
{
  // TODO: Take flags into account
  /*if( flags & RENDER_FLAG_NOOSD ) 
    return;*/

  //CSingleExit lock(g_graphicsContext);
  
  if (m_oldVideoRect != m_destRect)
  {
    g_application.GetComponent<CApplicationPlayer>()->SetPosition(m_sourceRect, m_destRect, m_viewRect);
    m_oldVideoRect = m_destRect;
  }

  
}

bool CWinDsRenderer::Supports(ESCALINGMETHOD method)
{
  if(method == VS_SCALINGMETHOD_NEAREST
  || method == VS_SCALINGMETHOD_LINEAR)
    return true;

  return false;
}

bool CWinDsRenderer::Supports( ERENDERFEATURE method )
{
  if ( method == RENDERFEATURE_CONTRAST
    || method == RENDERFEATURE_BRIGHTNESS
	|| method == RENDERFEATURE_ZOOM
	|| method == RENDERFEATURE_VERTICAL_SHIFT
	|| method == RENDERFEATURE_PIXEL_RATIO
	|| method == RENDERFEATURE_POSTPROCESS)
    return true;

  return false;
}

#endif
