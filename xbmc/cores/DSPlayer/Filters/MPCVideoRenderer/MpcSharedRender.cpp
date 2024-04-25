/*
 *      Copyright (C) 2005-2014 Team XBMC
 *      http://xbmc.org
 *
 *      Copyright (C) 2014-2015 Aracnoz
 *      http://github.com/aracnoz/xbmc
 *
 *      Copyright (C) 2024 Ti-BEN (Based on EvrSharedRenderer)
 *      http://github.com/tiben20/xbmc
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */
#include "MpcSharedRender.h"
#include "windowing/GraphicContext.h"
#include "windowing/windows/WinSystemWin32DX.h"
#include "Application/Application.h"
#include "application/ApplicationComponents.h"
#include "application/ApplicationPlayer.h"
#include "settings/AdvancedSettings.h"
#include "application/Application.h"

CMpcSharedRender::CMpcSharedRender()
{
  g_application.GetComponent<CApplicationPlayer>()->Register(this);
}

CMpcSharedRender::~CMpcSharedRender()
{
  g_application.GetComponent<CApplicationPlayer>()->Unregister(this);
}

HRESULT CMpcSharedRender::Render(DS_RENDER_LAYER layer)
{
  // Lock EVR thread while kodi rendering
  if (m_bWaitKodiRendering)
    m_dsWait.Wait(100);

  if (!g_application.GetComponent<CApplicationPlayer>()->ReadyDS() || (CServiceBroker::GetWinSystem()->GetGfxContext().IsFullScreenVideo() && layer == RENDER_LAYER_UNDER))
    return S_FALSE;

  // Render the GUI on EVR
  RenderInternal(layer);

  // Pull the trigger on the wait for the main Kodi application thread
  if (layer == RENDER_LAYER_OVER)
    m_kodiWait.Unlock();

  return S_OK;
}

void CMpcSharedRender::BeginRender()
{
  // Wait that MPC complete the rendering
  m_kodiWait.Lock();
  m_kodiWait.Wait(100);

  // Lock MPC thread while kodi rendering
  m_dsWait.Lock();

  // Clear RenderTarget
  /*ID3D11DeviceContext* pContext = DX::DeviceResources::Get()->GetD3DContext();
  ID3D11RenderTargetView* pSurface11;

  DX::DeviceResources::Get()->GetD3DDevice()->CreateRenderTargetView(m_pMPCUnderTexture.Get(), NULL, &pSurface11);
  D3DSetDebugName(pSurface11, "MPCUnderTexture render target");
  pContext->ClearRenderTargetView(pSurface11, m_fColor);
  pSurface11->Release();

  DX::DeviceResources::Get()->GetD3DDevice()->CreateRenderTargetView(m_pMPCOverTexture.Get(), NULL, &pSurface11);
  D3DSetDebugName(pSurface11, "MPCOverTexture render target");
  pContext->ClearRenderTargetView(pSurface11, m_fColor);
  pSurface11->Release();*/
  // Reset RenderCount
  ResetRenderCount();
}

void CMpcSharedRender::RenderToTexture(DS_RENDER_LAYER layer)
{
  m_currentVideoLayer = layer;

 
  ID3D11RenderTargetView* pSurface11;

  //DX::DeviceResources::Get()->GetD3DDevice()->CreateRenderTargetView(layer == RENDER_LAYER_UNDER ? m_pMPCUnderTexture.Get() : m_pMPCOverTexture.Get(), NULL, &pSurface11);
  //DX::DeviceResources::Get()->GetD3DContext()->OMSetRenderTargets(1, &pSurface11, 0);
  //pSurface11->Release();
}


void CMpcSharedRender::EndRender()
{

  // Force to complete the rendering on Kodi device
  //DX::DeviceResources::Get()->FinishCommandList();

  //do we need staging?
  //ForceComplete();

  m_bGuiVisible = GuiVisible();
  m_bGuiVisibleOver = GuiVisible(RENDER_LAYER_OVER);

  // Unlock EVR rendering
  m_dsWait.Unlock();
}
