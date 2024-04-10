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

#if HAS_DS_PLAYER

#include "URL.h"
#include "FilterSelectionRule.h"
#include "video/VideoInfoTag.h"
#include "utils/StreamDetails.h"
#include "settings/Settings.h"
#include "utils/log.h"
#include "utils/DSFileUtils.h"
#include "ServiceBroker.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"

CFilterSelectionRule::CFilterSelectionRule(TiXmlElement* pRule, const std::string &nodeName)
{
  Initialize(pRule, nodeName);
}

CFilterSelectionRule::~CFilterSelectionRule()
{}

void CFilterSelectionRule::Initialize(TiXmlElement* pRule, const std::string &nodeName)
{
  if (!pRule)
    return;

  ;
  if (!CDSXMLUtils::GetString(pRule, "name", &m_name))
    m_name = "un-named";

  CDSXMLUtils::GetTristate(pRule, "dxva", &m_dxva);

  m_mimeTypes = CDSXMLUtils::GetString(pRule, "mimetypes");
  m_fileName = CDSXMLUtils::GetString(pRule, "filename");

  m_audioCodec = CDSXMLUtils::GetString(pRule, "audiocodec");
  m_audioChannels = CDSXMLUtils::GetString(pRule, "audiochannels");
  m_videoCodec = CDSXMLUtils::GetString(pRule, "videocodec");
  m_videoResolution = CDSXMLUtils::GetString(pRule, "videoresolution");
  m_videoAspect = CDSXMLUtils::GetString(pRule, "videoaspect");
  m_videoFourcc = CDSXMLUtils::GetString(pRule, "fourcc");

  m_bStreamDetails = m_audioCodec.length() > 0 || m_audioChannels.length() > 0 || m_videoFourcc.length() > 0 ||
    m_videoCodec.length() > 0 || m_videoResolution.length() > 0 || m_videoAspect.length() > 0;

  if (m_bStreamDetails && !CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::SETTING_MYVIDEOS_EXTRACTFLAGS))
  {
    CLog::Log(LOGWARNING, "CFilterSelectionRule::Initialize: rule: {} needs media flagging, which is disabled", m_name.c_str());
  }

  m_filterName = CDSXMLUtils::GetString(pRule, "filter");

  TiXmlElement* pSubRule = pRule->FirstChildElement(nodeName);
  while (pSubRule)
  {
    vecSubRules.push_back(new CFilterSelectionRule(pSubRule, nodeName));
    pSubRule = pSubRule->NextSiblingElement(nodeName);
  }
}

bool CFilterSelectionRule::CompileRegExp(const std::string& str, CRegExp& regExp) const
{
  return str.length() > 0 && regExp.RegComp(str.c_str());
}

bool CFilterSelectionRule::MatchesRegExp(const std::string& str, CRegExp& regExp) const
{
  return regExp.RegFind(str, 0) != -1; // Need more testing
}

void CFilterSelectionRule::GetFilters(const CFileItem& item, std::vector<std::string> &vecCores, bool dxva)
{
  //CLog::Log(LOGDEBUG, "CFilterSelectionRule::GetFilters: considering rule: {}", m_name.c_str());

  if (m_bStreamDetails && (!item.HasVideoInfoTag())) 
    return;
  /*
  if (m_tAudio >= 0 && (m_tAudio > 0) != item.IsAudio()) return;
  if (m_tVideo >= 0 && (m_tVideo > 0) != item.IsVideo()) return;
  if (m_tInternetStream >= 0 && (m_tInternetStream > 0) != item.IsInternetStream()) return;

  if (m_tDVD >= 0 && (m_tDVD > 0) != item.IsDVD()) return;
  if (m_tDVDFile >= 0 && (m_tDVDFile > 0) != item.IsDVDFile()) return;
  if (m_tDVDImage >= 0 && (m_tDVDImage > 0) != item.IsDVDImage()) return;*/

  if (m_dxva >= 0 && (m_dxva > 0) != dxva) return;

  CRegExp regExp;

  if (m_bStreamDetails)
  {
    if (!item.GetVideoInfoTag()->HasStreamDetails())
    {
      CLog::Log(LOGDEBUG, "CFilterSelectionRule::GetFilters: cannot check rule: {}, no StreamDetails", m_name.c_str());
      return;
    }

    CStreamDetails streamDetails = item.GetVideoInfoTag()->m_streamDetails;

    if (CompileRegExp(m_audioCodec, regExp) && !MatchesRegExp(streamDetails.GetAudioCodec(), regExp)) return;

    if (CompileRegExp(m_videoCodec, regExp) && !MatchesRegExp(streamDetails.GetVideoCodec(), regExp)) return;

    if (CompileRegExp(m_videoFourcc, regExp) && !MatchesRegExp(streamDetails.GetVideoFourcc(), regExp)) return;

    if (CompileRegExp(m_videoResolution, regExp) &&
      !MatchesRegExp(CStreamDetails::VideoDimsToResolutionDescription(streamDetails.GetVideoWidth(),
      streamDetails.GetVideoHeight()), regExp)) return;

    if (CompileRegExp(m_videoAspect, regExp) && !MatchesRegExp(CStreamDetails::VideoAspectToAspectDescription(
      item.GetVideoInfoTag()->m_streamDetails.GetVideoAspect()), regExp)) return;
  }

  CURL url(item.GetPath());

  //if (CompileRegExp(m_fileTypes, regExp) && !MatchesRegExp(url.GetFileType(), regExp)) return;

  //if (CompileRegExp(m_protocols, regExp) && !MatchesRegExp(url.GetProtocol(), regExp)) return;

  if (CompileRegExp(m_mimeTypes, regExp) && !MatchesRegExp(item.GetMimeType(), regExp)) return;

  if (CompileRegExp(m_fileName, regExp) && !MatchesRegExp(item.GetPath(), regExp)) return;

  //CLog::Log(LOGDEBUG, "CFilterSelectionRule::GetFilters: matches rule: {}", m_name.c_str());

  for (unsigned int i = 0; i < vecSubRules.size(); i++)
    vecSubRules[i]->GetFilters(item, vecCores, dxva);

  if (!m_filterName.empty())
  {
    CLog::Log(LOGDEBUG, "CFilterSelectionRule::GetFilters: adding filter: {} for rule: {}", m_filterName.c_str(), m_name.c_str());
    vecCores.push_back(m_filterName);
  }
}

#endif