/*
 * (C) 2006-2014 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "madVRAllocatorPresenter.h"
#include "windowing/windows/WinSystemWin32DX.h"
#include <moreuuids.h>
#include "RendererSettings.h"
#include "guilib/GUIWindowManager.h"
#include "settings/Settings.h"
#include "settings/DisplaySettings.h"
#include "settings/AdvancedSettings.h"
#include "cores/DSPlayer/Filters/MadvrSettings.h"
#include "PixelShaderList.h"
#include "DSPlayer.h"
#include "utils/log.h"
#include "utils/SystemInfo.h"
#include "ServiceBroker.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"

using namespace KODI::MESSAGING;

#define ShaderStage_PreScale 0
#define ShaderStage_PostScale 1

extern bool g_bExternalSubtitleTime;

//
// CmadVRAllocatorPresenter
//

CmadVRAllocatorPresenter::CmadVRAllocatorPresenter(HWND hWnd, HRESULT& hr, std::string& _Error)
  : ISubPicAllocatorPresenterImpl(hWnd, hr)
  , m_ScreenSize(0, 0)
  , m_bIsFullscreen(false)
  , m_pMadvrShared(nullptr)
  , m_pD3DDev(nullptr)
{

  //Init Variable
  m_exclusiveCallback = ExclusiveCallback;
  m_firstBoot = true;
  m_isEnteringExclusive = false;
  m_kodiGuiDirtyAlgo = CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_guiAlgorithmDirtyRegions;
  m_activeVideoRect.SetRect(0, 0, 0, 0);
  m_frameCount = 0;
  
  if (FAILED(hr)) {
    _Error += "%s ISubPicAllocatorPresenterImpl failed\n";
    return;
  }

  hr = S_OK;
}

CmadVRAllocatorPresenter::~CmadVRAllocatorPresenter()
{
  if (m_pSRCB) {
    // nasty, but we have to let it know about our death somehow
    ((CSubRenderCallback*)(ISubRenderCallback2*)m_pSRCB)->SetDXRAP(nullptr);
  }
  
  if (m_pORCB) {
    // nasty, but we have to let it know about our death somehow
    ((COsdRenderCallback*)(IOsdRenderCallback*)m_pORCB)->SetDXRAP(nullptr);
  }

  // Unregister madVR Exclusive Callback
  if (Com::SmartQIPtr<IMadVRExclusiveModeCallback> pEXL = m_pDXR)
    pEXL->Unregister(m_exclusiveCallback, this);

  // Let's madVR restore original display mode (when adjust refresh it's handled by madVR)
  if (CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::SETTING_VIDEOPLAYER_ADJUSTREFRESHRATE) == ADJUST_REFRESHRATE_OFF)
  {
    if (Com::SmartQIPtr<IMadVRCommand> pMadVrCmd = m_pDXR)
      pMadVrCmd->SendCommand("restoreDisplayModeNow");
  }

  CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_guiAlgorithmDirtyRegions = m_kodiGuiDirtyAlgo;
  
  // the order is important here
  SAFE_DELETE(m_pMadvrShared);
  g_application.GetComponent<CApplicationPlayer>()->Unregister(this);
  m_pSubPicQueue = nullptr;
  m_pAllocator = nullptr;
  m_pDXR = nullptr;
  m_pORCB = nullptr;
  m_pSRCB = nullptr;

  CLog::Log(LOGDEBUG, "%s Resources released", __FUNCTION__);
}

STDMETHODIMP CmadVRAllocatorPresenter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
  if (riid != IID_IUnknown && m_pDXR) {
    if (SUCCEEDED(m_pDXR->QueryInterface(riid, ppv))) {
      return S_OK;
    }
  }

  return __super::NonDelegatingQueryInterface(riid, ppv);
}

void CmadVRAllocatorPresenter::SetResolution() const
{
  ULONGLONG frameRate;
  float fps;

  if (Com::SmartQIPtr<IMadVRInfo> pInfo = m_pDXR)
  {
    pInfo->GetUlonglong("frameRate", &frameRate);
    fps = 10000000.0 / frameRate;
  }

  SIZE nativeVideoSize = GetVideoSize(false);

  if (CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::SETTING_VIDEOPLAYER_ADJUSTREFRESHRATE) != ADJUST_REFRESHRATE_OFF && CServiceBroker::GetWinSystem()->GetGfxContext().IsFullScreenRoot())
  {
    RESOLUTION res = CResolutionUtils::ChooseBestResolution(fps, nativeVideoSize.cx, false);   
    bool bChanged = SetResolutionInternal(res);
    CLog::Log(LOGDEBUG, "%s change resolution %s", __FUNCTION__, bChanged ?  "<success>" : "<failed>");
  }
}

void CmadVRAllocatorPresenter::ExclusiveCallback(LPVOID context, int event)
{
  CmadVRAllocatorPresenter *pThis = (CmadVRAllocatorPresenter*)context;

  std::vector<std::string> strEvent = { "IsAboutToBeEntered", "WasJustEntered", "IsAboutToBeLeft", "WasJustLeft" };

  if (event == ExclusiveModeIsAboutToBeEntered || event == ExclusiveModeIsAboutToBeLeft)
    pThis->m_isEnteringExclusive = true;

  if (event == ExclusiveModeWasJustEntered || event == ExclusiveModeWasJustLeft)
    pThis->m_isEnteringExclusive = false;

  CLog::Log(LOGDEBUG, "%s madVR %s in Fullscreen Exclusive-Mode", __FUNCTION__, strEvent[event - 1].c_str());
}

void CmadVRAllocatorPresenter::EnableExclusive(bool bEnable)
{
  if (Com::SmartQIPtr<IMadVRCommand> pMadVrCmd = m_pDXR)
    pMadVrCmd->SendCommandBool("disableExclusiveMode", !bEnable);
};

void CmadVRAllocatorPresenter::ConfigureMadvr()
{
  // Disable SeekBar
  if (Com::SmartQIPtr<IMadVRCommand> pMadVrCmd = m_pDXR)
    pMadVrCmd->SendCommandBool("disableSeekbar", true);

  // Delay Playback
  m_pSettingsManager->SetBool("delayPlaybackStart2", CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::SETTING_DSPLAYER_DELAYMADVRPLAYBACK));

  if (Com::SmartQIPtr<IMadVRExclusiveModeCallback> pEXL = m_pDXR)
    pEXL->Register(m_exclusiveCallback, this);

  // Exclusive Mode
  if (CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::SETTING_DSPLAYER_EXCLUSIVEMODE))
  {
      m_pSettingsManager->SetBool("exclusiveDelay", true);
      m_pSettingsManager->SetBool("enableExclusive", true);
  }
  else
  {
    if (Com::SmartQIPtr<IMadVRCommand> pMadVrCmd = m_pDXR)
      pMadVrCmd->SendCommandBool("disableExclusiveMode", true);
  }

  // Direct3D Mode
  int iD3DMode = CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::SETTING_DSPLAYER_D3DPRESNTATION);
  switch (iD3DMode)
  {
  case MADVR_D3D9:
  {
    m_pSettingsManager->SetBool("useD3d11", false);
    break;
  }
  case MADVR_D3D11_VSYNC:
  {
    m_pSettingsManager->SetBool("useD3d11", true);
    m_pSettingsManager->SetBool("d3d11noDelay", true);
    break;
  }
  case MADVR_D3D11_NOVSYNC:
  {
    m_pSettingsManager->SetBool("useD3d11", true);
    m_pSettingsManager->SetBool("d3d11noDelay", false);
    break;
  }
  }

  // Pre-Presented Frames
  int iNumPresentWindowed = CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::SETTING_DSPLAYER_NUMPRESENTWINDOWED);
  int iNumPresentExclusive = CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::SETTING_DSPLAYER_NUMPRESENTEXCLUSIVE);

  if (iNumPresentWindowed > 0)
    m_pSettingsManager->SetInt("preRenderFramesWindowed", iNumPresentWindowed);

  if (iNumPresentExclusive > 0)
    m_pSettingsManager->SetInt("preRenderFrames", iNumPresentExclusive);
}

bool CmadVRAllocatorPresenter::ParentWindowProc(HWND hWnd, UINT uMsg, WPARAM *wParam, LPARAM *lParam, LRESULT *ret) const
{
  if (Com::SmartQIPtr<IMadVRSubclassReplacement> pMVRSR = m_pDXR)
    return (pMVRSR->ParentWindowProc(hWnd, uMsg, wParam, lParam, ret) != 0);
  else
    return false;
}

void CmadVRAllocatorPresenter::DisplayChange(bool bExternalChange)
{
  CAutoLock cAutoLock(this);

  if (DX::DeviceResources::Get()->GetD3DContext() == nullptr || m_pD3DDev == nullptr)
    return;

  CLog::Log(LOGDEBUG, "%s need to re-create the shared textures", __FUNCTION__);

  if (m_pMadvrShared != nullptr)
    SAFE_DELETE(m_pMadvrShared);

  MONITORINFO mi;
  mi.cbSize = sizeof(MONITORINFO);
  if (GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &mi)) {
    m_ScreenSize.SetSize(mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top);
    m_activeVideoRect.SetRect(0, 0, m_ScreenSize.cx, m_ScreenSize.cy);
  }

  m_pMadvrShared = DNew CMadvrSharedRender();
  m_pMadvrShared->CreateTextures(DX::DeviceResources::Get()->GetD3DDevice(), m_pD3DDev, (int)m_ScreenSize.cx, (int)m_ScreenSize.cy);
}

STDMETHODIMP CmadVRAllocatorPresenter::ClearBackground(LPCSTR name, REFERENCE_TIME frameStart, RECT *fullOutputRect, RECT *activeVideoRect)
{
  CAutoLock cAutoLock(this);

  if (m_pMadvrShared != nullptr)
    return m_pMadvrShared->Render(RENDER_LAYER_UNDER);
  else
    return CALLBACK_INFO_DISPLAY;
}

STDMETHODIMP CmadVRAllocatorPresenter::RenderOsd(LPCSTR name, REFERENCE_TIME frameStart, RECT *fullOutputRect, RECT *activeVideoRect)
{
  CAutoLock cAutoLock(this);

  if (CServiceBroker::GetWinSystem()->GetGfxContext().IsFullScreenVideo())
    m_activeVideoRect.SetRect(activeVideoRect->left, activeVideoRect->top, activeVideoRect->right, activeVideoRect->bottom);

  if (m_pMadvrShared != nullptr)
    return m_pMadvrShared->Render(RENDER_LAYER_OVER);
  else
    return CALLBACK_INFO_DISPLAY;
}

STDMETHODIMP CmadVRAllocatorPresenter::SetDeviceOsd(IDirect3DDevice9* pD3DDev)
{
  if (!pD3DDev)
  {
    // release all resources
    m_pSubPicQueue = nullptr;
    m_pAllocator = nullptr;
  }
  return S_OK;
}

HRESULT CmadVRAllocatorPresenter::SetDevice(IDirect3DDevice9* pD3DDev)
{
  CLog::Log(LOGDEBUG, "%s madVR's device it's ready", __FUNCTION__);

  if (!pD3DDev)
  {
    // release all resources
    m_pSubPicQueue = nullptr;
    m_pAllocator = nullptr;
    return S_OK;
  }

  m_pD3DDev = (IDirect3DDevice9Ex*)pD3DDev;

  if (m_firstBoot)
  {
    MONITORINFO mi;
    mi.cbSize = sizeof(MONITORINFO);
    if (GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &mi)) {
      m_ScreenSize.SetSize(mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top);
      m_activeVideoRect.SetRect(0, 0, m_ScreenSize.cx, m_ScreenSize.cy);
    }

    m_pMadvrShared = DNew CMadvrSharedRender();
    m_pMadvrShared->CreateTextures(DX::DeviceResources::Get()->GetD3DDevice(), m_pD3DDev, (int)m_ScreenSize.cx, (int)m_ScreenSize.cy);

    m_firstBoot = false;
    CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_guiAlgorithmDirtyRegions = DIRTYREGION_SOLVER_FILL_VIEWPORT_ALWAYS;
  }

  Com::SmartSize size(m_ScreenSize.cx,m_ScreenSize.cy);

  if (m_pAllocator) {
    m_pAllocator->ChangeDevice(pD3DDev);
  }
  else
  {
    m_pAllocator = DNew CDX9SubPicAllocator(pD3DDev, size, true);
    if (!m_pAllocator) {
      return E_FAIL;
    }
  }

  HRESULT hr = S_OK;

  if (!m_pSubPicQueue) {
    CAutoLock cAutoLock(this);
    m_pSubPicQueue = g_dsSettings.pRendererSettings->subtitlesSettings.bufferAhead > 0
      ? (ISubPicQueue*)DNew CSubPicQueue(g_dsSettings.pRendererSettings->subtitlesSettings.bufferAhead, g_dsSettings.pRendererSettings->subtitlesSettings.disableAnimations, m_pAllocator, &hr)
      : (ISubPicQueue*)DNew CSubPicQueueNoThread(m_pAllocator, &hr);
  }
  else {
    m_pSubPicQueue->Invalidate();
  }

  if (SUCCEEDED(hr) && (m_SubPicProvider)) {
    m_pSubPicQueue->SetSubPicProvider(m_SubPicProvider);
  }

  return hr;
}

HRESULT CmadVRAllocatorPresenter::Render( REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, REFERENCE_TIME atpf, int left, int top, int right, int bottom, int width, int height)
{
  Com::SmartRect wndRect(0, 0, width, height);
  Com::SmartRect videoRect(left, top, right, bottom);

  __super::SetPosition(wndRect, videoRect);

  if (!g_bExternalSubtitleTime) {
    SetTime(rtStart);
  }
  if (atpf > 0 && m_pSubPicQueue) {
    m_fps = 10000000.0 / atpf;
    m_pSubPicQueue->SetFPS(m_fps);
  }

  if (!g_application.GetComponent<CApplicationPlayer>()->IsRenderingVideo())
  {
    m_NativeVideoSize = GetVideoSize(false);
    m_AspectRatio = GetVideoSize(true);

    // Configure Render Manager
    g_application.GetComponent<CApplicationPlayer>()->Configure(m_NativeVideoSize.cx, m_NativeVideoSize.cy, m_AspectRatio.cx, m_AspectRatio.cy, m_fps, CONF_FLAGS_FULLSCREEN);
    CLog::Log(LOGDEBUG, "%s Render manager configured (FPS: %f) %i %i %i %i", __FUNCTION__, m_fps, m_NativeVideoSize.cx, m_NativeVideoSize.cy, m_AspectRatio.cx, m_AspectRatio.cy);
  }

  // Reset madVR Stats at start (after 50 frames)
  if (m_frameCount > -1)
  {
    m_frameCount += 1;

    if (m_frameCount > 50)
    {
      INPUT ip;
      ip.type = INPUT_KEYBOARD;
      ip.ki.wScan = 0;
      ip.ki.time = 0;
      ip.ki.dwExtraInfo = 0;

      // Press
      ip.ki.wVk = VK_CONTROL;
      ip.ki.dwFlags = 0; // 0 for key press
      SendInput(1, &ip, sizeof(INPUT));

      ip.ki.wVk = 'R';
      ip.ki.dwFlags = 0;
      SendInput(1, &ip, sizeof(INPUT));

      // Release
      ip.ki.wVk = 'R';
      ip.ki.dwFlags = KEYEVENTF_KEYUP;
      SendInput(1, &ip, sizeof(INPUT));

      ip.ki.wVk = VK_CONTROL;
      ip.ki.dwFlags = KEYEVENTF_KEYUP;
      SendInput(1, &ip, sizeof(INPUT));

      m_frameCount = -1;
    }
  }
 
  AlphaBltSubPic(Com::SmartSize(width, height));
  return S_OK;
}

// ISubPicAllocatorPresenter
STDMETHODIMP CmadVRAllocatorPresenter::CreateRenderer(IUnknown** ppRenderer)
{
  CheckPointer(ppRenderer, E_POINTER);

  if (m_pDXR) {
    return E_UNEXPECTED;
  }

  m_pDXR.CoCreateInstance(CLSID_madVR, GetOwner());
  if (!m_pDXR) {
    return E_FAIL;
  }

  // Init Settings Manager
  m_pSettingsManager = DNew CMadvrSettingsManager(m_pDXR);

  Com::SmartQIPtr<ISubRender> pSR = m_pDXR;
  if (!pSR) {
    m_pDXR = nullptr;
    return E_FAIL;
  }

  m_pSRCB = DNew CSubRenderCallback(this);
  if (FAILED(pSR->SetCallback(m_pSRCB))) {
    m_pDXR = nullptr;
    return E_FAIL;
  }

  // IOsdRenderCallback
  Com::SmartQIPtr<IMadVROsdServices> pOR = m_pDXR;
  if (!pOR) {
    m_pDXR = nullptr;
    return E_FAIL;
  }

  m_pORCB = DNew COsdRenderCallback(this);
  if (FAILED(pOR->OsdSetRenderCallback("Kodi.Gui", m_pORCB))) {
    m_pDXR = nullptr;
    return E_FAIL;
  }

  if (Com::SmartQIPtr<IMadVRSubclassReplacement> pMVRSR = m_pDXR)
    VERIFY(SUCCEEDED(pMVRSR->DisableSubclassing()));

  // resize madVR
  if (Com::SmartQIPtr<IVideoWindow> pVW = m_pDXR)
  {
    RECT w;
    w.left = 0;
    w.top = 0;
    w.right = CServiceBroker::GetWinSystem()->GetGfxContext().GetWidth();
    w.bottom = CServiceBroker::GetWinSystem()->GetGfxContext().GetHeight();
    pVW->SetWindowPosition(w.left, w.top, w.right - w.left, w.bottom - w.top);

    // madVR supports calling IVideoWindow::put_Owner before the pins are connected
    pVW->put_Owner((OAHWND)CDSPlayer::GetDShWnd());
  }

  // Configure initial Madvr Settings
  ConfigureMadvr();

  g_application.GetComponent<CApplicationPlayer>()->Register(this);

  (*ppRenderer = (IUnknown*)(INonDelegatingUnknown*)(this))->AddRef();

  return S_OK;
}

void CmadVRAllocatorPresenter::SetPosition(CRect sourceRect, CRect videoRect, CRect viewRect)
{
  Com::SmartRect wndR(viewRect.x1, viewRect.y1, viewRect.x2, viewRect.y2);
  Com::SmartRect videoR(videoRect.x1, videoRect.y1, videoRect.x2, videoRect.y2);
  SetPosition(wndR, videoR);
}

STDMETHODIMP_(void) CmadVRAllocatorPresenter::SetPosition(RECT w, RECT v)
{
  if (!CServiceBroker::GetWinSystem()->GetGfxContext().IsFullScreenVideo())
  {
    w.left = 0;
    w.top = 0;
    w.right = CServiceBroker::GetWinSystem()->GetGfxContext().GetWidth();
    w.bottom = CServiceBroker::GetWinSystem()->GetGfxContext().GetHeight();
  }

  RENDER_STEREO_MODE stereoMode = CServiceBroker::GetWinSystem()->GetGfxContext().GetStereoMode();
  switch (stereoMode)
  {
  case RENDER_STEREO_MODE_SPLIT_VERTICAL:
  {
    w.right *= 2;
    v.right *= 2;
    break;
  }
  case RENDER_STEREO_MODE_SPLIT_HORIZONTAL:
  {
    w.bottom *= 2;
    v.bottom *= 2;
    break;
  }
  }

  if (Com::SmartQIPtr<IBasicVideo> pBV = m_pDXR) {
    pBV->SetDefaultSourcePosition();
    pBV->SetDestinationPosition(v.left, v.top, v.right - v.left, v.bottom - v.top);
  }

  if (Com::SmartQIPtr<IVideoWindow> pVW = m_pDXR) {
    pVW->SetWindowPosition(w.left, w.top, w.right - w.left, w.bottom - w.top);
  }
}

STDMETHODIMP_(SIZE) CmadVRAllocatorPresenter::GetVideoSize(bool fCorrectAR)
{
  SIZE size = { 0, 0 };

  if (!fCorrectAR) {
    if (Com::SmartQIPtr<IBasicVideo> pBV = m_pDXR) {
      pBV->GetVideoSize(&size.cx, &size.cy);
    }
  }
  else {
    if (Com::SmartQIPtr<IBasicVideo2> pBV2 = m_pDXR) {
      pBV2->GetPreferredAspectRatio(&size.cx, &size.cy);
    }
  }

  return size;
}

STDMETHODIMP CmadVRAllocatorPresenter::GetDIB(BYTE* lpDib, DWORD* size)
{
  HRESULT hr = E_NOTIMPL;
  if (Com::SmartQIPtr<IBasicVideo> pBV = m_pDXR) {
    hr = pBV->GetCurrentImage((long*)size, (long*)lpDib);
  }
  return hr;
}

STDMETHODIMP_(bool) CmadVRAllocatorPresenter::Paint(bool fAll)
{
  return false;
}

void CmadVRAllocatorPresenter::SetPixelShader() const
{
  g_dsSettings.pixelShaderList->UpdateActivatedList();
  m_shaderStage = 0;
  std::string strStage;
  PixelShaderVector& psVec = g_dsSettings.pixelShaderList->GetActivatedPixelShaders();

  for (PixelShaderVector::iterator it = psVec.begin();
    it != psVec.end(); it++)
  {
    CExternalPixelShader *Shader = *it;
    Shader->Load();
    m_shaderStage = Shader->GetStage();
    m_shaderStage == 0 ? strStage = "Pre-Resize" : strStage = "Post-Resize";
    SetPixelShader(Shader->GetSourceData().c_str(), nullptr);
    Shader->DeleteSourceData();

    CLog::Log(LOGDEBUG, "%s Set PixelShader: %s applied: %s", __FUNCTION__, Shader->GetName().c_str(), strStage.c_str());
  }
};

STDMETHODIMP CmadVRAllocatorPresenter::SetPixelShader(LPCSTR pSrcData, LPCSTR pTarget)
{
  HRESULT hr = E_NOTIMPL;
  if (Com::SmartQIPtr<IMadVRExternalPixelShaders> pEPS = m_pDXR) {
    if (!pSrcData && !pTarget) {
      hr = pEPS->ClearPixelShaders(false);
    }
    else {
      hr = pEPS->AddPixelShader(pSrcData, pTarget, m_shaderStage, nullptr);
    }
  }
  return hr;
}

bool CmadVRAllocatorPresenter::SetResolutionInternal(const RESOLUTION res, bool forceChange /*= false*/)
{
  MONITORINFOEX mi;
  mi.cbSize = sizeof(MONITORINFOEX);
  GetMonitorInfo(MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTONEAREST), &mi);

  DEVMODE sDevMode;
  ZeroMemory(&sDevMode, sizeof(sDevMode));
  sDevMode.dmSize = sizeof(sDevMode);

  RESOLUTION_INFO res_info = CDisplaySettings::GetInstance().GetResolutionInfo(res);
  CLog::Log(LOGDEBUG, "%s set system resolution to %s", __FUNCTION__, res_info.strMode.c_str());

  // If we can't read the current resolution or any detail of the resolution is different than res
  if (!EnumDisplaySettings(mi.szDevice, ENUM_CURRENT_SETTINGS, &sDevMode) ||
    sDevMode.dmPelsWidth != res_info.iWidth || sDevMode.dmPelsHeight != res_info.iHeight ||
    sDevMode.dmDisplayFrequency != static_cast<int>(res_info.fRefreshRate) ||
    ((sDevMode.dmDisplayFlags & DM_INTERLACED) && !(res_info.dwFlags & D3DPRESENTFLAG_INTERLACED)) ||
    (!(sDevMode.dmDisplayFlags & DM_INTERLACED) && (res_info.dwFlags & D3DPRESENTFLAG_INTERLACED))
    || forceChange)
  {
    ZeroMemory(&sDevMode, sizeof(sDevMode));
    sDevMode.dmSize = sizeof(sDevMode);
    sDevMode.dmDriverExtra = 0;
    sDevMode.dmPelsWidth = res_info.iWidth;
    sDevMode.dmPelsHeight = res_info.iHeight;
    sDevMode.dmDisplayFrequency = static_cast<int>(res_info.fRefreshRate);
    sDevMode.dmDisplayFlags = (res_info.dwFlags & D3DPRESENTFLAG_INTERLACED) ? DM_INTERLACED : 0;
    sDevMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY | DM_DISPLAYFLAGS;

    LONG rc;
    bool bResChanged = false;

    // Windows 8 refresh rate workaround for 24.0, 48.0 and 60.0 Hz
    if ( !CSysInfo::IsWindowsVersionAtLeast(CSysInfo::WindowsVersionWin10) 
      && (res_info.fRefreshRate == 24.0 || res_info.fRefreshRate == 48.0 || res_info.fRefreshRate == 60.0))
    {
      CLog::Log(LOGDEBUG, "%s : Using Windows 8+ workaround for refresh rate %d Hz", __FUNCTION__, static_cast<int>(res_info.fRefreshRate));

      // Get current resolution stored in registry
      DEVMODE sDevModeRegistry;
      ZeroMemory(&sDevModeRegistry, sizeof(sDevModeRegistry));
      sDevModeRegistry.dmSize = sizeof(sDevModeRegistry);
      if (EnumDisplaySettings(mi.szDevice, ENUM_REGISTRY_SETTINGS, &sDevModeRegistry))
      {
        // Set requested mode in registry without actually changing resolution
        rc = ChangeDisplaySettingsEx(mi.szDevice, &sDevMode, nullptr, CDS_UPDATEREGISTRY | CDS_NORESET, nullptr);
        if (rc == DISP_CHANGE_SUCCESSFUL)
        {
          // Change resolution based on registry setting
          rc = ChangeDisplaySettingsEx(mi.szDevice, nullptr, nullptr, CDS_FULLSCREEN, nullptr);
          if (rc == DISP_CHANGE_SUCCESSFUL)
            bResChanged = true;
          else
            CLog::Log(LOGERROR, "%s : ChangeDisplaySettingsEx (W8+ change resolution) failed with %d, using fallback", __FUNCTION__, rc);

          // Restore registry with original values
          sDevModeRegistry.dmSize = sizeof(sDevModeRegistry);
          sDevModeRegistry.dmDriverExtra = 0;
          sDevModeRegistry.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY | DM_DISPLAYFLAGS;
          rc = ChangeDisplaySettingsEx(mi.szDevice, &sDevModeRegistry, nullptr, CDS_UPDATEREGISTRY | CDS_NORESET, nullptr);
          if (rc != DISP_CHANGE_SUCCESSFUL)
            CLog::Log(LOGERROR, "%s : ChangeDisplaySettingsEx (W8+ restore registry) failed with %d", __FUNCTION__, rc);
        }
        else
          CLog::Log(LOGERROR, "%s : ChangeDisplaySettingsEx (W8+ set registry) failed with %d, using fallback", __FUNCTION__, rc);
      }
      else
        CLog::Log(LOGERROR, "%s : Unable to retrieve registry settings for Windows 8+ workaround, using fallback", __FUNCTION__);
    }

    // Standard resolution change/fallback for Windows 8+ workaround
    if (!bResChanged)
    {
      // CDS_FULLSCREEN is for temporary fullscreen mode and prevents icons and windows from moving
      // to fit within the new dimensions of the desktop
      rc = ChangeDisplaySettingsEx(mi.szDevice, &sDevMode, nullptr, CDS_FULLSCREEN, nullptr);
      if (rc == DISP_CHANGE_SUCCESSFUL)
        bResChanged = true;
      else
        CLog::Log(LOGERROR, "%s : ChangeDisplaySettingsEx failed with %d", __FUNCTION__, rc);
    }
    return bResChanged;
  }

  // nothing to do, return success
  return true;
}