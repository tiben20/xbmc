#pragma once
/*
 *      Copyright (C) 2005-2008 Team XBMC
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
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef HAS_DS_PLAYER
#error DSPlayer's header file included without HAS_DS_PLAYER defined
#endif

#include "utils/XMLUtils.h"
#include "utils/RegExp.h"
#include "FileItem.h"
#include "StreamsManager.h"

class TiXmlElement;

class CFilterSelectionRule
{
public:
  CFilterSelectionRule(TiXmlElement* rule, const std::string &nodeName);
  virtual ~CFilterSelectionRule();

  void GetFilters(const CFileItem& item, std::vector<std::string> &vecCores, bool dxva = false);

private:
  bool CompileRegExp(const std::string& str, CRegExp& regExp) const;
  bool MatchesRegExp(const std::string& str, CRegExp& regExp) const;
  void Initialize(TiXmlElement* pRule, const std::string &nodeName);

  std::string m_name;

  std::string m_mimeTypes;
  std::string m_fileName;

  std::string m_audioCodec;
  std::string m_audioChannels;
  std::string m_videoCodec;
  std::string m_videoResolution;
  std::string m_videoAspect;
  std::string m_videoFourcc;

  std::string m_filterName;
  bool m_bStreamDetails;

  std::vector<CFilterSelectionRule *> vecSubRules;
  int m_dxva;
};
