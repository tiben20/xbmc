/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
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

#if HAS_DS_PLAYER

#include "RenderDSManager.h"
#include "cores/VideoPlayer/Videorenderers/RenderFlags.h"
#include "threads/SingleLock.h"
#include "utils/log.h"
#include "utils/StringUtils.h"

#include "application/Application.h"
#include "messaging/ApplicationMessenger.h"
#include "settings/AdvancedSettings.h"
#include "settings/DisplaySettings.h"
#include "settings/MediaSettings.h"
#include "settings/Settings.h"
#include "windowing/GraphicContext.h"
#include "cores/DataCacheCore.h"
#include "GraphFilters.h"

#include "DSGraph.h"
#include "StreamsManager.h"

#include "utils/CPUInfo.h"

//#include "windowing/windows/WinSystemWin32DX.h"

using namespace KODI::MESSAGING;

CRenderDSManager::CRenderDSManager(IRenderDSMsg *player) :
  m_pRenderer(nullptr),
  m_bTriggerUpdateResolution(false),
  m_bTriggerDisplayChange(false),
  m_renderDebug(false),
  m_bWaitingForRenderOnDS(true),
  m_bPreInit(false),
  m_Resolution(RES_INVALID),
  m_renderState(STATE_UNCONFIGURED),
  m_displayLatency(0.0),
  m_width(0),
  m_height(0),
  m_dwidth(0),
  m_dheight(0),
  m_fps(0.0f),
  m_playerPort(player)
{
}

CRenderDSManager::~CRenderDSManager()
{
  delete m_pRenderer;
}

void CRenderDSManager::GetVideoRect(CRect &source, CRect &dest, CRect &view)
{
  CSingleExit lock(m_statelock);
  if (m_pRenderer)
    m_pRenderer->GetVideoRect(source, dest, view);
}

float CRenderDSManager::GetAspectRatio()
{
  CSingleExit lock(m_statelock);
  if (m_pRenderer)
    return m_pRenderer->GetAspectRatio();
  else
    return 1.0f;
}

bool CRenderDSManager::Configure(unsigned int width, unsigned int height, unsigned int d_width, unsigned int d_height, float fps, unsigned flags)
{
  // check if something has changed
  {
    CSingleExit lock(m_statelock);

    if (m_width == width &&
      m_height == height &&
      m_dwidth == d_width &&
      m_dheight == d_height &&
      m_fps == fps &&
      (m_flags & ~CONF_FLAGS_FULLSCREEN) == (flags & ~CONF_FLAGS_FULLSCREEN) &&
      m_pRenderer != NULL)
      return true;
  }

  CLog::Log(LOGDEBUG, "CRenderDSManager::Configure - change configuration. %dx%d. display: %dx%d. framerate: %4.2f.", width, height, d_width,d_height, fps);
  {
    CSingleExit lock(m_statelock);
    m_width = width;
    m_height = height;
    m_dwidth = d_width;
    m_dheight = d_height;
    m_fps = fps;
    m_flags = flags;
    m_renderState = STATE_CONFIGURING;
    m_stateEvent.Reset();
    m_bWaitingForRenderOnDS = true;
  }

  if (!m_stateEvent.WaitMSec(1000))
  {
    CLog::Log(LOGWARNING, "CRenderDSManager::Configure - timeout waiting for configure");
    return false;
  }

  CSingleExit lock(m_statelock);
  if (m_renderState != STATE_CONFIGURED)
  {
    CLog::Log(LOGWARNING, "CRenderDSManager::Configure - failed to configure");
    return false;
  }

  return true;
}

bool CRenderDSManager::Configure()
{
  CSingleExit lock(m_statelock);
  CSingleExit lock3(m_datalock);

  /*
  if (m_pRenderer && m_pRenderer->GetRenderFormat() != m_format)
  {
    DeleteRenderer();
  }
  */

  if(!m_pRenderer)
  {
    CreateRenderer();
    if (!m_pRenderer)
      return false;
  }

  bool result = m_pRenderer->Configure(m_width, m_height, m_dwidth, m_dheight, m_fps, m_flags,(ERenderFormat)0,0,0);
  if (result)
  {
    CRenderInfo info = m_pRenderer->GetRenderInfo();
    int renderbuffers = info.optimal_buffer_size;

    m_pRenderer->Update();
    m_bTriggerUpdateResolution = true;
    m_renderState = STATE_CONFIGURED;
  }
  else
    m_renderState = STATE_UNCONFIGURED;

  m_stateEvent.Set();
  m_playerPort->VideoParamsChange();
  return result;
}

void CRenderDSManager::Reset()
{
  if (m_pRenderer)
    m_pRenderer->Reset();
}

bool CRenderDSManager::IsConfigured() const
{
  CSingleExit lock(m_statelock);
  if (m_renderState == STATE_CONFIGURED)
    return true;
  else
    return false;
}

void CRenderDSManager::Update()
{
  if (m_pRenderer)
    m_pRenderer->Update();
}


bool CRenderDSManager::HasFrame()
{
  if (!IsConfigured())
    return false;

    return true;
}

void CRenderDSManager::FrameMove()
{
  UpdateResolution();

  {
    CSingleExit lock(m_statelock);

    if (m_renderState == STATE_UNCONFIGURED)
      return;
    else if (m_renderState == STATE_CONFIGURING)
    {
      lock.Leave();
      if (!Configure())
        return;

      if (m_flags & CONF_FLAGS_FULLSCREEN)
      {
        CApplicationMessenger::GetInstance().PostMsg(TMSG_SWITCHTOFULLSCREEN);
      }
    }
    if (m_renderState == STATE_CONFIGURED && m_bWaitingForRenderOnDS && g_graphicsContext.IsFullScreenVideo())
    {
      m_bWaitingForRenderOnDS = false;
      m_bPreInit = false;
      m_playerPort->SetRenderOnDS(true);
    }
  }
}

void CRenderDSManager::EndRender()
{
  if (m_renderState == STATE_CONFIGURED && !g_application.GetComponent<CApplicationPlayer>()->ReadyDS())
    g_graphicsContext.Clear(0);
}

void CRenderDSManager::PreInit()
{
  if (!g_application.IsCurrentThread())
  {
    CLog::Log(LOGERROR, "CRenderDSManager::UnInit - not called from render thread");
    return;
  }

  CSingleExit lock(m_statelock);

  if (!m_pRenderer)
    CreateRenderer();

  UpdateDisplayLatency();

  m_bPreInit = true;
}

void CRenderDSManager::UnInit()
{
  if (!g_application.IsCurrentThread())
  {
    CLog::Log(LOGERROR, "CRenderDSManager::UnInit - not called from render thread");
    return;
  }

  CSingleExit lock(m_statelock);

  m_debugRenderer.Flush();

  DeleteRenderer();

  m_renderState = STATE_UNCONFIGURED;
}

bool CRenderDSManager::Flush()
{
  if (!m_pRenderer)
    return true;

  if (g_application.IsCurrentThread())
  {
    CLog::Log(LOGDEBUG, "%s - flushing renderer", __FUNCTION__);


    CSingleExit exitlock(g_graphicsContext);

    CSingleExit lock(m_statelock);
    CSingleExit lock3(m_datalock);

    if (m_pRenderer)
    {
      m_pRenderer->Flush();
      m_debugRenderer.Flush();
      m_flushEvent.Set();
    }
  }
  else
  {
    m_flushEvent.Reset();
    CApplicationMessenger::GetInstance().PostMsg(TMSG_RENDERER_FLUSH);
    if (!m_flushEvent.WaitMSec(1000))
    {
      CLog::Log(LOGERROR, "%s - timed out waiting for renderer to flush", __FUNCTION__);
      return false;
    }
    else
      return true;
  }
  return true;
}

void CRenderDSManager::CreateRenderer()
{
  if (!m_pRenderer)
  {
    m_pRenderer = new CWinDsRenderer();

    if (m_pRenderer)
      m_pRenderer->PreInit();
    else
      CLog::Log(LOGERROR, "RenderDSManager::CreateRenderer: failed to create renderer");
  }
}

void CRenderDSManager::DeleteRenderer()
{
  CLog::Log(LOGDEBUG, "%s - deleting renderer", __FUNCTION__);

  if (m_pRenderer)
  {
    delete m_pRenderer;
    m_pRenderer = NULL;
  }
}

void CRenderDSManager::SetViewMode(int iViewMode)
{
  CSingleExit lock(m_statelock);
  if (m_pRenderer)
    m_pRenderer->SetViewMode(iViewMode);
  m_playerPort->VideoParamsChange();
}

RESOLUTION CRenderDSManager::GetResolution()
{
  RESOLUTION res = g_graphicsContext.GetVideoResolution();

  CSingleExit lock(m_statelock);
  if (m_renderState == STATE_UNCONFIGURED)
    return res;

  if (CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::SETTING_VIDEOPLAYER_ADJUSTREFRESHRATE) != ADJUST_REFRESHRATE_OFF)
    res = CResolutionUtils::ChooseBestResolution(m_fps, m_width, CONF_FLAGS_STEREO_MODE_MASK(m_flags));

  return res;
}

void CRenderDSManager::Render(bool clear, DWORD flags, DWORD alpha, bool gui)
{
  CSingleExit exitLock(g_graphicsContext);

  {
    CSingleExit lock(m_statelock);
    if (m_renderState != STATE_CONFIGURED)
      return;
  }

  g_application.GetComponent<CApplicationPlayer>()->RenderToTexture(RENDER_LAYER_OVER);

  if (!gui && m_pRenderer->IsGuiLayer())
    return;

  if (!gui || m_pRenderer->IsGuiLayer())
  {
      PresentSingle(clear, flags, alpha);
  }

  if (gui)
  {
    if (!m_pRenderer->IsGuiLayer())
      m_pRenderer->Update();

    CRect src, dst, view;
    m_pRenderer->GetVideoRect(src, dst, view);

    if (m_renderDebug)
    {
      std::string audio, video, player, cores;

      m_playerPort->GetDebugInfo(audio, video, player);

      cores = StringUtils::Format("W( %s )", g_cpuInfo.GetCoresUsageString().c_str());

      m_debugRenderer.SetInfo(audio, video, player, cores);
      m_debugRenderer.Render(src, dst, view);

      m_debugTimer.Set(1000);
    }
  }
}

bool CRenderDSManager::IsGuiLayer()
{
  { CSingleExit lock(m_statelock);

    if (!m_pRenderer)
      return false;

    if (m_pRenderer->IsGuiLayer() && HasFrame())
      return true;

    if (m_renderDebug && m_debugTimer.IsTimePast())
      return true;
  }
  return false;
}

bool CRenderDSManager::IsVideoLayer()
{
  { CSingleExit lock(m_statelock);

    if (!m_pRenderer)
      return false;

    if (!m_pRenderer->IsGuiLayer())
      return true;
  }
  return false;
}

/* simple present method */
void CRenderDSManager::PresentSingle(bool clear, DWORD flags, DWORD alpha)
{
  m_pRenderer->RenderUpdate(clear, flags, alpha);
}

void CRenderDSManager::UpdateDisplayLatency()
{
  float refresh = g_graphicsContext.GetFPS();
  if (g_graphicsContext.GetVideoResolution() == RES_WINDOW)
    refresh = 0; // No idea about refresh rate when windowed, just get the default latency
  m_displayLatency = (double) g_advancedSettings.GetDisplayLatency(refresh);

  if (CGraphFilters::Get()->GetAuxAudioDelay())
    m_displayLatency += (double)g_advancedSettings.GetDisplayAuxDelay(refresh);

  g_application.GetComponent<CApplicationPlayer>()->SetAVDelay(CMediaSettings::GetInstance().GetCurrentVideoSettings().m_AudioDelay);

  CLog::Log(LOGDEBUG, "CRenderDSManager::UpdateDisplayLatency - Latency set to %1.0f msec", m_displayLatency * 1000.0f);
}

void CRenderDSManager::UpdateResolution()
{
  if (m_bTriggerDisplayChange)
  {
    if (m_Resolution != RES_INVALID)
    {      
      CLog::Log(LOGDEBUG, "%s gui resolution updated by external display change event", __FUNCTION__);
      g_graphicsContext.SetVideoResolution(m_Resolution);
      UpdateDisplayLatency();
    }
    m_bTriggerDisplayChange = false;
    m_playerPort->VideoParamsChange();
  }
  if (m_bTriggerUpdateResolution)
  {
    if (g_graphicsContext.IsFullScreenVideo() && g_graphicsContext.IsFullScreenRoot())
    {
      if (CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::SETTING_VIDEOPLAYER_ADJUSTREFRESHRATE) != ADJUST_REFRESHRATE_OFF && m_fps > 0.0f)
      {
        if (m_Resolution == RES_INVALID)
          m_Resolution = CResolutionUtils::ChooseBestResolution(m_fps, m_width, CONF_FLAGS_STEREO_MODE_MASK(m_flags) != 0);

        g_graphicsContext.SetVideoResolution(m_Resolution);
        UpdateDisplayLatency();
      }
      m_bTriggerUpdateResolution = false;
    }
    m_playerPort->VideoParamsChange();
  }
}

void CRenderDSManager::DisplayChange(bool bExternalChange)
{
  // Get Current Display settings
  MONITORINFOEX mi;
  mi.cbSize = sizeof(MONITORINFOEX);
  GetMonitorInfo(MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTONEAREST), &mi);

  DEVMODE dm;
  ZeroMemory(&dm, sizeof(dm));
  dm.dmSize = sizeof(dm);
  if (EnumDisplaySettingsEx(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm, 0) == FALSE)
    EnumDisplaySettingsEx(mi.szDevice, ENUM_REGISTRY_SETTINGS, &dm, 0);

  int width = dm.dmPelsWidth;
  int height = dm.dmPelsHeight;
  bool bInterlaced = (dm.dmDisplayFlags & DM_INTERLACED) ? true : false;
  int iRefreshRate = dm.dmDisplayFrequency;
  float refreshRate;
  if (iRefreshRate == 59 || iRefreshRate == 29 || iRefreshRate == 23)
    refreshRate = static_cast<float>(iRefreshRate + 1) / 1.001f;
  else
    refreshRate = static_cast<float>(iRefreshRate);

  // Convert Current Resolution to Kodi Res
  std::string sRes = StringUtils::Format("%1i%05i%05i%09.5f%s", g_Windowing.GetCurrentScreen(), width, height, refreshRate, bInterlaced ? "istd" : "pstd");
  RESOLUTION res = CDisplaySettings::GetResolutionFromString(sRes);
  RESOLUTION_INFO res_info = CDisplaySettings::GetInstance().GetResolutionInfo(res);

  if (bExternalChange)
  {
    if (m_bPreInit)
    {
      m_bPreInit = false;
      m_playerPort->SetDSWndVisible(true);
      CLog::Log(LOGDEBUG, "%s showing dsplayer window", __FUNCTION__);
    }

    m_Resolution = res;
    m_bTriggerDisplayChange = true;
    CLog::Log(LOGDEBUG, "%s external display change event update resolution to %s", __FUNCTION__, res_info.strMode.c_str());
  }
  else
  {
    CLog::Log(LOGDEBUG, "%s internal display change event update resolution to %s", __FUNCTION__, res_info.strMode.c_str());
    if (m_bTriggerDisplayChange)
    {  
      m_bTriggerDisplayChange = false;
      CLog::Log(LOGDEBUG, "%s requested gui resolution update by external display change event dropped", __FUNCTION__);
    }
  }
}

void CRenderDSManager::TriggerUpdateResolution(float fps, int width, int flags)
{
  if (width)
  {
    m_fps = fps;
    m_width = width;
    m_flags = flags;
  }
  m_bTriggerUpdateResolution = true;
}

void CRenderDSManager::ToggleDebug()
{
  m_renderDebug = !m_renderDebug;
  m_debugTimer.SetExpired();
}

bool CRenderDSManager::Supports(ERENDERFEATURE feature)
{
  CSingleExit lock(m_statelock);
  if (m_pRenderer)
    return m_pRenderer->Supports(feature);
  else
    return false;
}

bool CRenderDSManager::Supports(ESCALINGMETHOD method)
{
  CSingleExit lock(m_statelock);
  if (m_pRenderer)
    return m_pRenderer->Supports(method);
  else
    return false;
}


#endif