#pragma once

/*
 *      Copyright (C) 2005-2010 Team XBMC
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

#ifndef HAS_DS_PLAYER
#error DSPlayer's header file included without HAS_DS_PLAYER defined
#endif

#include <tinyxml.h>
#include "utils/log.h"
#include "FilterSelectionRule.h"
#include "ShadersSelectionRule.h"
#include "filesystem/DllLibCurl.h"
#include "utils/RegExp.h"
#include "URL.h"
#include "video/VideoInfoTag.h"
#include "utils/StreamDetails.h"
#include "utils/DSFileUtils.h"

class CGlobalFilterSelectionRule
{
public:
  CGlobalFilterSelectionRule(TiXmlElement* pRule, int iPriority)
    : m_url(false)
  {
    Initialize(pRule, iPriority);
  }

  ~CGlobalFilterSelectionRule()
  {
    delete m_pSource;
    delete m_pSplitter;
    delete m_pAudio;
    delete m_pVideo;
    delete m_pSubs;
    delete m_pExtras;
    delete m_pAudioRenderer;
    delete m_pShaders;

    //CLog::Log(LOGDEBUG, "%s Ressources released", __FUNCTION__); Log Spam
  }

  bool Match(const CFileItem& pFileItem, bool checkUrl = false)
  {
    CURL url(pFileItem.GetPath());
    CRegExp regExp;

    if (m_fileTypes.empty() && m_fileName.empty() && m_Protocols.empty() && m_videoCodec.empty())
    {
      CLog::Log(LOGDEBUG, "%s: no Rule parameters", __FUNCTION__);
      return false;
    }

    CStreamDetails streamDetails;
    if (m_bStreamDetails)
    {
      if (!pFileItem.HasVideoInfoTag() || !pFileItem.GetVideoInfoTag()->HasStreamDetails())
      {
        CLog::Log(LOGDEBUG, "%s: %s, no StreamDetails", __FUNCTION__, m_name.c_str());
        return false;
      }
      streamDetails = pFileItem.GetVideoInfoTag()->m_streamDetails;
    }

    if (!checkUrl && m_url > 0) return false;
    if (checkUrl && pFileItem.IsInternetStream() && m_url < 1) return false;
    if (CompileRegExp(m_fileTypes, regExp) && !MatchesRegExp(url.GetFileType(), regExp)) return false;
    if (CompileRegExp(m_fileName, regExp) && !MatchesRegExp(pFileItem.GetPath(), regExp)) return false;
    if (CompileRegExp(m_Protocols, regExp) && !MatchesRegExp(url.GetProtocol(), regExp)) return false;
    if (CompileRegExp(m_videoCodec, regExp) && !MatchesRegExp(streamDetails.GetVideoCodec(), regExp)) return false;
    std::string audioCodec = streamDetails.GetAudioCodec();
    if (audioCodec == "dca")
      audioCodec = "dts";
    if (CompileRegExp(m_audioCodec, regExp) && !MatchesRegExp(audioCodec, regExp)) return false;

    return true;
  }

  void GetSourceFilters(const CFileItem& item, std::vector<std::string> &vecCores)
  {
    m_pSource->GetFilters(item, vecCores);
  }

  void GetSplitterFilters(const CFileItem& item, std::vector<std::string> &vecCores)
  {
    m_pSplitter->GetFilters(item, vecCores);
  }

  void GetAudioRendererFilters(const CFileItem& item, std::vector<std::string> &vecCores)
  {
    m_pAudioRenderer->GetFilters(item, vecCores, false);
  }

  void GetVideoFilters(const CFileItem& item, std::vector<std::string> &vecCores, bool dxva = false)
  {
    m_pVideo->GetFilters(item, vecCores, dxva);
  }

  void GetAudioFilters(const CFileItem& item, std::vector<std::string> &vecCores, bool dxva = false)
  {
    m_pAudio->GetFilters(item, vecCores, dxva);
  }

  void GetSubsFilters(const CFileItem& item, std::vector<std::string> &vecCores, bool dxva = false)
  {
    m_pSubs->GetFilters(item, vecCores, dxva);
  }

  void GetExtraFilters(const CFileItem& item, std::vector<std::string> &vecCores, bool dxva = false)
  {
    m_pExtras->GetFilters(item, vecCores, dxva);
  }

  void GetShaders(const CFileItem& item, std::vector<uint32_t>& shaders, std::vector<uint32_t>& shadersStages, bool dxva = false)
  {
    m_pShaders->GetShaders(item, shaders, shadersStages, dxva);
  }

  std::string GetPriority()
  {
    return m_priority;
  }

private:
  int        m_url;
  bool       m_bStreamDetails;
  std::string m_name;
  std::string m_fileName;
  std::string m_fileTypes;
  std::string m_Protocols;
  std::string m_videoCodec;
  std::string m_audioCodec;
  std::string m_priority;
  CFilterSelectionRule * m_pSource;
  CFilterSelectionRule * m_pSplitter;
  CFilterSelectionRule * m_pVideo;
  CFilterSelectionRule * m_pAudio;
  CFilterSelectionRule * m_pSubs;
  CFilterSelectionRule * m_pExtras;
  CFilterSelectionRule * m_pAudioRenderer;
  CShadersSelectionRule * m_pShaders;

  int GetTristate(const char* szValue) const
  {
    if (szValue)
    {
      if (stricmp(szValue, "true") == 0) return 1;
      if (stricmp(szValue, "false") == 0) return 0;
    }
    return -1;
  }
  bool CompileRegExp(const std::string& str, CRegExp& regExp) const
  {
    return str.length() > 0 && regExp.RegComp(str.c_str());
  }
  bool MatchesRegExp(const std::string& str, CRegExp& regExp) const
  {
    return regExp.RegFind(str, 0) == 0;
  }

  void Initialize(TiXmlElement* pRule, int iPriority)
  {
    if ( !CDSXMLUtils::GetString(pRule, "name", &m_name))
      m_name = "un-named";

    CDSXMLUtils::GetTristate(pRule, "url", &m_url);
    m_fileTypes = CDSXMLUtils::GetString(pRule, "filetypes");
    m_fileName = CDSXMLUtils::GetString(pRule, "filename");
    m_Protocols = CDSXMLUtils::GetString(pRule, "protocols");
    m_videoCodec = CDSXMLUtils::GetString(pRule, "videocodec");
    m_audioCodec = CDSXMLUtils::GetString(pRule, "audiocodec");
    m_bStreamDetails = m_videoCodec.length() > 0 || m_audioCodec.length() > 0;

    if (!CDSXMLUtils::GetString(pRule, "priority", &m_priority))
      m_priority = StringUtils::Format("%i", iPriority);

    // Source rules
    m_pSource = new CFilterSelectionRule(pRule->FirstChildElement("source"), "source");

    // Splitter rules
    m_pSplitter = new CFilterSelectionRule(pRule->FirstChildElement("splitter"), "splitter");

    // Audio rules
    m_pAudio = new CFilterSelectionRule(pRule->FirstChildElement("audio"), "audio");

    // Video rules
    m_pVideo = new CFilterSelectionRule(pRule->FirstChildElement("video"), "video");

    // Subs rules
    m_pSubs = new CFilterSelectionRule(pRule->FirstChildElement("subs"), "subs");

    // Extra rules
    m_pExtras = new CFilterSelectionRule(pRule->FirstChildElement("extra"), "extra");

    // Audio renderer rules
    m_pAudioRenderer = new CFilterSelectionRule(pRule->FirstChildElement("audiorenderer"), "audiorenderer");

    // Shaders
    m_pShaders = new CShadersSelectionRule(pRule->FirstChildElement("shaders"));
  }
};