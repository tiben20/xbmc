/*
 *      Copyright (C) 2005-2010 Team XBMC
 *      http://www.xbmc.org
 *
 *		Copyright (C) 2010-2013 Eduard Kytmanov
 *		http://www.avmedia.su
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

#include "RendererSettings.h"
#include "filesystem/file.h"
#include "utils/log.h"
#include "util.h"
#include "profiles/ProfileManager.h"
#include "settings/Settings.h"
#include "utils/XMLUtils.h"
#include "utils/SystemInfo.h"
#include "PixelShaderList.h"
#include "utils/StringUtils.h"
#include "ServiceBroker.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"

using namespace XFILE;
CDSSettings::CDSSettings(void)
{
  m_hD3DX9Dll = NULL;
  pRendererSettings = NULL;
  isEVR = false;

  m_pDwmIsCompositionEnabled = NULL;
  m_pDwmEnableComposition = NULL;
  m_hDWMAPI = LoadLibrary(L"dwmapi.dll");
  if (m_hDWMAPI)
  {
    (FARPROC &)m_pDwmIsCompositionEnabled = GetProcAddress(m_hDWMAPI, "DwmIsCompositionEnabled");
    (FARPROC &)m_pDwmEnableComposition = GetProcAddress(m_hDWMAPI, "DwmEnableComposition");
  }
}

void CDSSettings::Initialize(std::string renderer)
{
  CStdStringA videoRender = renderer;

  if (videoRender.ToLower() == "madvr")
    pRendererSettings = new CMADVRRendererSettings();
  
  if (videoRender.ToLower() == "mpcvr")
    pRendererSettings = new CMPCVRSettings();

  // Create the pixel shader list
  pixelShaderList.reset(new CPixelShaderList());
  pixelShaderList->Load();
}

CDSSettings::~CDSSettings(void)
{
  if (m_hD3DX9Dll)
    FreeLibrary(m_hD3DX9Dll);

  if (m_hDWMAPI)
    FreeLibrary(m_hDWMAPI);
}

void CDSSettings::LoadConfig()
{
  std::string strDsConfigFile = CServiceBroker::GetSettingsComponent()->GetProfileManager()->GetUserDataItem("dsplayer/renderersettings.xml");
  if (!CFile::Exists(strDsConfigFile))
  {
    CLog::Log(LOGINFO, "No renderersettings.xml to load ({})", strDsConfigFile.c_str());
    return;
  }
  
  // load the xml file
  CXBMCTinyXML xmlDoc;

  if (!xmlDoc.LoadFile(strDsConfigFile))
  {
    CLog::Log(LOGERROR, "Error loading {}, Line %d\n{}", strDsConfigFile.c_str(), xmlDoc.ErrorRow(), xmlDoc.ErrorDesc());
    return;
  }

  TiXmlElement *pRootElement = xmlDoc.RootElement();
  if (pRootElement && (strcmpi(pRootElement->Value(), "renderersettings") != 0))
  {
    CLog::Log(LOGERROR, "Error loading {}, no <renderersettings> node", strDsConfigFile.c_str());
    return;
  }

  // First, shared settings
  TiXmlElement *pElement = pRootElement->FirstChildElement("sharedsettings");
  if (pElement)
  {

    // Default values are set by SetDefault. We don't need a default parameter in GetXXX.
    // If XMLUtils::GetXXX fails, the value is not modified

    // VSync
    XMLUtils::GetBoolean(pElement, "VSync", pRendererSettings->vSync);
    XMLUtils::GetBoolean(pElement, "AlterativeVSync", pRendererSettings->alterativeVSync);
    XMLUtils::GetBoolean(pElement, "AccurateVSync", pRendererSettings->vSyncAccurate);
    XMLUtils::GetBoolean(pElement, "FlushGPUBeforeVSync", pRendererSettings->flushGPUBeforeVSync);
    XMLUtils::GetBoolean(pElement, "FlushGPUWait", pRendererSettings->flushGPUWait);
    XMLUtils::GetBoolean(pElement, "FlushGPUAfterPresent", pRendererSettings->flushGPUAfterPresent);
    XMLUtils::GetInt(pElement, "VSyncOffset", pRendererSettings->vSyncOffset, 0, 100);
  
    // Misc
    XMLUtils::GetBoolean(pElement, "DisableDesktopComposition", pRendererSettings->disableDesktopComposition);
  }

  // Subtitles
  pElement = pRootElement->FirstChildElement("subtitlessettings");
  if (pElement)
  {
    XMLUtils::GetBoolean(pElement, "ForcePowerOfTwoTextures", pRendererSettings->subtitlesSettings.forcePowerOfTwoTextures);
    XMLUtils::GetUInt(pElement, "BufferAhead", pRendererSettings->subtitlesSettings.bufferAhead);
    XMLUtils::GetBoolean(pElement, "DisableAnimations", pRendererSettings->subtitlesSettings.disableAnimations);
    pElement = pElement->FirstChildElement("TextureSize");
    if (pElement)
    {
      XMLUtils::GetUInt(pElement, "width", (uint32_t&) pRendererSettings->subtitlesSettings.textureSize.cx);
      XMLUtils::GetUInt(pElement, "height", (uint32_t&) pRendererSettings->subtitlesSettings.textureSize.cy);
    }
  }
}

HINSTANCE CDSSettings::GetD3X9Dll()
{
if (m_hD3DX9Dll == NULL)
  {
    int min_ver = D3DX_SDK_VERSION;
    int max_ver = D3DX_SDK_VERSION;
    
    m_nDXSdkRelease = 0;

    if(D3DX_SDK_VERSION >= 42) {
      // August 2009 SDK (v42) is not compatible with older versions
      min_ver = 42;      
    } else {
      if(D3DX_SDK_VERSION > 33) {
        // versions between 34 and 41 have no known compatibility issues
        min_ver = 34;
      }  else {    
        // The minimum version that supports the functionality required by MPC is 24
        min_ver = 24;
  
        if(D3DX_SDK_VERSION == 33) {
          // The April 2007 SDK (v33) should not be used (crash sometimes during shader compilation)
          max_ver = 32;    
        }
      }
    }
    
    // load latest compatible version of the DLL that is available
    for (int i=max_ver; i>=min_ver; i--)
    {
      m_strD3DX9Version = StringUtils::Format(_T("d3dx9_%d.dll"), i);
      m_hD3DX9Dll = LoadLibrary (m_strD3DX9Version.c_str());
      if (m_hD3DX9Dll) 
      {
        m_nDXSdkRelease = i;
        break;
      }
    }
  }

  return m_hD3DX9Dll;
}

CDSSettings g_dsSettings;
bool g_bNoDuration = false;
bool g_bExternalSubtitleTime = false;

#endif