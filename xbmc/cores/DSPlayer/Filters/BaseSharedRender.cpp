/*
 *      Copyright (C) 2005-2014 Team XBMC
 *      http://xbmc.org
 *
 *      Copyright (C) 2014-2015 Aracnoz
 *      http://github.com/aracnoz/xbmc
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */
#include "BaseSharedRender.h"
#include "Utils/Log.h"
#include "guilib/GUIWindowManager.h"
#include "windowing/windows/WinSystemWin32DX.h"
#include "settings/AdvancedSettings.h"
#include "application/Application.h"
#include "threads/SingleLock.h"
#include "threads/SystemClock.h"
#include "dsutil/DShowCommon.h"

const DWORD D3DFVF_VID_FRAME_VERTEX = 0x004 | 0x100;

using namespace XbmcThreads;

struct VID_FRAME_VERTEX
{
  float x;
  float y;
  float z;
  float rhw;
  float u;
  float v;
};

CRenderWait::CRenderWait()
{
  m_renderState = RENDERFRAME_UNLOCK;
}

void CRenderWait::Wait(int ms)
{
  XbmcThreads::EndTime<> timeout(ms);
  CSingleExit lock(m_presentlock);
  while (m_renderState == RENDERFRAME_LOCK && !timeout.IsTimePast())
    m_presentevent.wait(lock, timeout.MillisLeft());
}

void CRenderWait::Lock()
{
  CSingleExit lock(m_presentlock);
  m_renderState = RENDERFRAME_LOCK;
}

void CRenderWait::Unlock()
{
  {
    CSingleExit lock(m_presentlock);
    m_renderState = RENDERFRAME_UNLOCK;
  }
  m_presentevent.notifyAll();
}

void CBaseSharedRender::IncRenderCount()
{
  if (!g_application.GetComponent<CApplicationPlayer>()->ReadyDS())
    return;

  m_currentVideoLayer == RENDER_LAYER_UNDER ? m_renderUnderCount += 1 : m_renderOverCount += 1;
}

void CBaseSharedRender::ResetRenderCount()
{
  m_renderUnderCount = 0;
  m_renderOverCount = 0;
}

bool CBaseSharedRender::GuiVisible(DS_RENDER_LAYER layer /* = RENDER_LAYER_ALL */)
{
  switch (layer)
  {
  case RENDER_LAYER_UNDER:
    return m_renderUnderCount > 0;
  case RENDER_LAYER_OVER:
    return m_renderOverCount > 0;
  case RENDER_LAYER_ALL:
    return m_renderOverCount + m_renderUnderCount > 0;
  }

  return false;
}

HRESULT CBaseSharedRender::CreateFakeStaging(ID3D11Texture2D** ppTexture)
{
  D3D11_TEXTURE2D_DESC Desc;
  Desc.Width = 1;
  Desc.Height = 1;
  Desc.MipLevels = 1;
  Desc.ArraySize = 1;
  Desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  Desc.SampleDesc.Count = 1;
  Desc.SampleDesc.Quality = 0;
  Desc.Usage = D3D11_USAGE_STAGING;
  Desc.BindFlags = 0;
  Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  Desc.MiscFlags = 0;

  return m_pD3DDeviceKodi->CreateTexture2D(&Desc, NULL, ppTexture);
}

HRESULT CBaseSharedRender::ForceComplete()
{
  HRESULT hr = S_OK;
  D3D11_MAPPED_SUBRESOURCE region;
  D3D11_BOX UnitBox = { 0, 0, 0, 1, 1, 1 };
  ID3D11DeviceContext* pContext;

  m_pD3DDeviceKodi->GetImmediateContext(&pContext);
  pContext->CopySubresourceRegion(m_pKodiFakeStaging, 0, 0, 0, 0, m_pKodiUnderTexture, 0, &UnitBox);

  hr = pContext->Map(m_pKodiFakeStaging, 0, D3D11_MAP_READ, 0, &region);
  if (SUCCEEDED(hr))
  {
    pContext->Unmap(m_pKodiFakeStaging, 0);
    SAFE_RELEASE(pContext);
  }
  return hr;
}

CBaseSharedRender::CBaseSharedRender()
{
  color_t clearColour = g_Windowing.UseLimitedColor() ? (16 * 0x010101) : 0;
  CD3DHelper::XMStoreColor(m_fColor, clearColour);
  m_bWaitKodiRendering = !g_advancedSettings.m_bNotWaitKodiRendering;
}

CBaseSharedRender::~CBaseSharedRender()
{
  // release DSPlayer Renderer resources
  SAFE_RELEASE(m_pDSVertexBuffer);
  SAFE_RELEASE(m_pDSUnderTexture);
  SAFE_RELEASE(m_pDSOverTexture);

  // release Kodi resources
  SAFE_RELEASE(m_pKodiUnderTexture);
  SAFE_RELEASE(m_pKodiOverTexture);
  SAFE_RELEASE(m_pKodiFakeStaging);
}

HRESULT CBaseSharedRender::CreateSharedResource(IDirect3DTexture9** ppTexture9, ID3D11Texture2D** ppTexture11)
{
  HRESULT hr;
  HANDLE pSharedHandle = nullptr;

  if (FAILED(hr = m_pD3DDeviceDS->CreateTexture(m_dwWidth, m_dwHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, ppTexture9, &pSharedHandle)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceKodi->OpenSharedResource(pSharedHandle, __uuidof(ID3D11Texture2D), (void**)(ppTexture11))))
    return hr;

  return hr;
}

HRESULT CBaseSharedRender::CreateTextures(ID3D11Device* pD3DDeviceKodi, IDirect3DDevice9Ex* pD3DDeviceDS, int width, int height)
{
  HRESULT hr;
  m_pD3DDeviceKodi = pD3DDeviceKodi;
  m_pD3DDeviceDS = pD3DDeviceDS;
  m_dwWidth = width;
  m_dwHeight = height;

  // Create VertexBuffer
  if (FAILED(hr = m_pD3DDeviceDS->CreateVertexBuffer(sizeof(VID_FRAME_VERTEX) * 4, D3DUSAGE_WRITEONLY, D3DFVF_VID_FRAME_VERTEX, D3DPOOL_DEFAULT, &m_pDSVertexBuffer, NULL)))
    CLog::Log(LOGDEBUG, "%s Failed to create vertex buffer", __FUNCTION__);

  // Create Fake Staging Texture
  if (FAILED(hr = CreateFakeStaging(&m_pKodiFakeStaging)))
    CLog::Log(LOGDEBUG, "%s Failed to create fake staging texture", __FUNCTION__);

  // Create Under Shared Texture
  if (FAILED(hr = CreateSharedResource(&m_pDSUnderTexture, &m_pKodiUnderTexture)))
    CLog::Log(LOGDEBUG, "%s Failed to create under shared texture", __FUNCTION__);

  // Create Over Shared Texture
  if (FAILED(hr = CreateSharedResource(&m_pDSOverTexture, &m_pKodiOverTexture)))
    CLog::Log(LOGDEBUG, "%s Failed to create over shared texture", __FUNCTION__);

  return hr;
}

HRESULT CBaseSharedRender::RenderInternal(DS_RENDER_LAYER layer)
{
  HRESULT hr = E_UNEXPECTED;

  // If there isn't something to draw skip di rendering
  if (!m_bGuiVisible)
    return hr;

  // If the over layer it's empty skip the rendering of the under layer and drawn everything over DSPlayer renderer device
  if (layer == RENDER_LAYER_UNDER && !m_bGuiVisibleOver)
    return hr;

  if (layer == RENDER_LAYER_OVER)
    m_bGuiVisibleOver ? layer = RENDER_LAYER_OVER : layer = RENDER_LAYER_UNDER;

  // Store DSPlayer renderer States
  if (FAILED(hr = StoreDeviceState()))
    return hr;

  // Setup DSPlayer renderer Device
  if (FAILED(hr = SetupDeviceState()))
    return hr;

  // Setup Vertex Buffer
  if (FAILED(hr = SetupVertex()))
    return hr;

  // Draw Kodi shared texture on DSPlayer renderer D3D9 device
  if (FAILED(hr = RenderTexture(layer)))
    return hr;

  // Restore DSPlayer renderer states
  if (FAILED(hr = ReStoreDeviceState()))
    return hr;

  return hr;
}

HRESULT CBaseSharedRender::RenderTexture(DS_RENDER_LAYER layer)
{
  IDirect3DTexture9* pTexture9;

  layer == RENDER_LAYER_UNDER ? pTexture9 = m_pDSUnderTexture : pTexture9 = m_pDSOverTexture;

  HRESULT hr = m_pD3DDeviceDS->SetStreamSource(0, m_pDSVertexBuffer, 0, sizeof(VID_FRAME_VERTEX));
  if (FAILED(hr))
    return hr;

  hr = m_pD3DDeviceDS->SetTexture(0, pTexture9);
  if (FAILED(hr))
    return hr;

  hr = m_pD3DDeviceDS->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
  if (FAILED(hr))
    return hr;

  return hr;
}

HRESULT CBaseSharedRender::SetupVertex()
{
  VID_FRAME_VERTEX* vertices = nullptr;

  // Lock the vertex buffer
  HRESULT hr = m_pDSVertexBuffer->Lock(0, 0, (void**)&vertices, D3DLOCK_DISCARD);

  if (SUCCEEDED(hr))
  {
    RECT rDest;
    rDest.bottom = m_dwHeight;
    rDest.left = 0;
    rDest.right = m_dwWidth;
    rDest.top = 0;

    vertices[0].x = (float)rDest.left - 0.5f;
    vertices[0].y = (float)rDest.top - 0.5f;
    vertices[0].z = 0.0f;
    vertices[0].rhw = 1.0f;
    vertices[0].u = 0.0f;
    vertices[0].v = 0.0f;

    vertices[1].x = (float)rDest.right - 0.5f;
    vertices[1].y = (float)rDest.top - 0.5f;
    vertices[1].z = 0.0f;
    vertices[1].rhw = 1.0f;
    vertices[1].u = 1.0f;
    vertices[1].v = 0.0f;

    vertices[2].x = (float)rDest.right - 0.5f;
    vertices[2].y = (float)rDest.bottom - 0.5f;
    vertices[2].z = 0.0f;
    vertices[2].rhw = 1.0f;
    vertices[2].u = 1.0f;
    vertices[2].v = 1.0f;

    vertices[3].x = (float)rDest.left - 0.5f;
    vertices[3].y = (float)rDest.bottom - 0.5f;
    vertices[3].z = 0.0f;
    vertices[3].rhw = 1.0f;
    vertices[3].u = 0.0f;
    vertices[3].v = 1.0f;

    hr = m_pDSVertexBuffer->Unlock();
    if (FAILED(hr))
      return hr;
  }

  return hr;
}

HRESULT CBaseSharedRender::StoreDeviceState()
{
  HRESULT hr = E_UNEXPECTED;

  if (FAILED(hr = m_pD3DDeviceDS->GetScissorRect(&m_oldScissorRect)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->GetVertexShader(&m_pOldVS)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->GetFVF(&m_dwOldFVF)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->GetTexture(0, &m_pOldTexture)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->GetStreamSource(0, &m_pOldStreamData, &m_nOldOffsetInBytes, &m_nOldStride)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->GetRenderState(D3DRS_CULLMODE, &m_D3DRS_CULLMODE)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->GetRenderState(D3DRS_LIGHTING, &m_D3DRS_LIGHTING)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->GetRenderState(D3DRS_ZENABLE, &m_D3DRS_ZENABLE)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->GetRenderState(D3DRS_ALPHABLENDENABLE, &m_D3DRS_ALPHABLENDENABLE)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->GetRenderState(D3DRS_SRCBLEND, &m_D3DRS_SRCBLEND)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->GetRenderState(D3DRS_DESTBLEND, &m_D3DRS_DESTBLEND)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->GetPixelShader(&m_pPix)))
    return hr;

  return hr;
}

HRESULT CBaseSharedRender::SetupDeviceState()
{
  HRESULT hr = E_UNEXPECTED;

  RECT newScissorRect;
  newScissorRect.bottom = m_dwHeight;
  newScissorRect.top = 0;
  newScissorRect.left = 0;
  newScissorRect.right = m_dwWidth;

  if (FAILED(hr = m_pD3DDeviceDS->SetScissorRect(&newScissorRect)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->SetVertexShader(NULL)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->SetFVF(D3DFVF_VID_FRAME_VERTEX)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->SetPixelShader(NULL)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->SetRenderState(D3DRS_LIGHTING, FALSE)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->SetRenderState(D3DRS_ZENABLE, FALSE)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA)))
    return hr;

  return hr;
}

HRESULT CBaseSharedRender::ReStoreDeviceState()
{
  HRESULT hr = S_FALSE;

  if (FAILED(hr = m_pD3DDeviceDS->SetScissorRect(&m_oldScissorRect)))
    return hr;

  hr = m_pD3DDeviceDS->SetTexture(0, m_pOldTexture);

  if (m_pOldTexture)
    m_pOldTexture->Release();

  if (FAILED(hr))
    return hr;

  hr = m_pD3DDeviceDS->SetVertexShader(m_pOldVS);

  if (m_pOldVS)
    m_pOldVS->Release();

  if (FAILED(hr))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->SetFVF(m_dwOldFVF)))
    return hr;

  hr = m_pD3DDeviceDS->SetStreamSource(0, m_pOldStreamData, m_nOldOffsetInBytes, m_nOldStride);

  if (m_pOldStreamData)
    m_pOldStreamData->Release();

  if (FAILED(hr))
    return hr;

  hr = m_pD3DDeviceDS->SetPixelShader(m_pPix);
  if (m_pPix)
    m_pPix->Release();

  if (FAILED(hr))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->SetRenderState(D3DRS_CULLMODE, m_D3DRS_CULLMODE)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->SetRenderState(D3DRS_LIGHTING, m_D3DRS_LIGHTING)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->SetRenderState(D3DRS_ZENABLE, m_D3DRS_ZENABLE)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->SetRenderState(D3DRS_ALPHABLENDENABLE, m_D3DRS_ALPHABLENDENABLE)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->SetRenderState(D3DRS_SRCBLEND, m_D3DRS_SRCBLEND)))
    return hr;

  if (FAILED(hr = m_pD3DDeviceDS->SetRenderState(D3DRS_DESTBLEND, m_D3DRS_DESTBLEND)))
    return hr;

  return hr;
}